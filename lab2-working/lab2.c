/*
 *
 * CSEE 4840 Lab 2 for 2019
 *
 * Name/UNI: Zakiy Manigo (ztm2106) & Robel Wondwossen (rw3043)
 */
#include "fbputchar.h"
#include "usbkeyboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define SERVER_HOST "128.59.19.114"
#define SERVER_PORT 42000
#define BUFFER_SIZE 128

int sockfd;
struct libusb_device_handle *keyboard;
uint8_t endpoint_address;
pthread_t network_thread, cursor_thread;
void *network_thread_f(void *);
void *cursor_blink_thread(void *);

char keycode_to_ascii(uint8_t keycode, uint8_t modifiers) {
    const char keymap[] = "1234567890-=\tqwertyuiop[]\nasdfghjkl;'`\\zxcvbnm,./ ";
    const char shift_keymap[] = "!@#$%^&*()_+\tQWERTYUIOP{}|ASDFGHJKL:\"~|ZXCVBNM<>? ";

    if (keycode >= 4 && keycode <= 39) {
        return (modifiers & (USB_LSHIFT | USB_RSHIFT)) ? shift_keymap[keycode - 4] : keymap[keycode - 4];
    }

    switch (keycode) {
        case 0x2B: return '\t'; // Tab
        case 0x2C: return ' ';  // Space
        case 0x28: return '\n'; // Enter
        case 0x2A: return '\b'; // Backspace
        default: return 0;
    }
}

void *network_thread_f(void *ignored) {
    char recvBuf[BUFFER_SIZE];
    int n;
    while ((n = read(sockfd, recvBuf, BUFFER_SIZE - 1)) > 0) {
        recvBuf[n] = '\0';
        display_received_message(recvBuf);
    }
    return NULL;
}

void *cursor_blink_thread(void *arg) {
    int blink_state = 1;
    while (1) {
        if (blink_state) {
            draw_cursor(23, 2);
        } else {
            fbputchar(' ', 23, 2);
        }
        blink_state = !blink_state;
        usleep(500000); // 500ms
    }
    return NULL;
}

int main() {
    struct sockaddr_in serv_addr;
    struct usb_keyboard_packet packet;
    int transferred, input_col = 2, input_row = 23;
    char input_buffer[BUFFER_SIZE] = {0};

    fbopen();
    fbclear();
    fbputs("> ", 23, 0);

    keyboard = openkeyboard(&endpoint_address);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr);
    connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    pthread_create(&network_thread, NULL, network_thread_f, NULL);
    pthread_create(&cursor_thread, NULL, cursor_blink_thread, NULL);
    pthread_detach(cursor_thread);

    for (;;) {
        libusb_interrupt_transfer(keyboard, endpoint_address, (unsigned char *)&packet, sizeof(packet), &transferred, 0);
        if (transferred == sizeof(packet)) {
            char c = keycode_to_ascii(packet.keycode[0], packet.modifiers);
            if (c && input_col - 2 < BUFFER_SIZE - 1) { // 🔹 Buffer Protection
                fbputchar(c, input_row, input_col);
                input_buffer[input_col - 2] = c;
                input_col++;
            }
            if (packet.keycode[0] == 0x2A && input_col > 2) { // Backspace
                input_col--;
                fbputchar(' ', input_row, input_col);
                input_buffer[input_col - 2] = '\0';
            }
            if (packet.keycode[0] == 0x50 && input_col > 2) { // Left Arrow
                input_col--;
            }
            if (packet.keycode[0] == 0x4F && input_col < 64 && input_buffer[input_col - 2]) { // Right Arrow
                input_col++;
            }
            if (packet.keycode[0] == 0x28) { // Enter
                send(sockfd, input_buffer, strlen(input_buffer), 0);
                display_received_message(input_buffer);
                memset(input_buffer, 0, sizeof(input_buffer));
                fbclear_input_area();
                fbputs("> ", 23, 0);
                input_col = 2;
            }
            draw_cursor(input_row, input_col);
        }
    }

    pthread_cancel(network_thread);
    pthread_join(network_thread, NULL);
    return 0;
}
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
#define BUFFER_SIZE 264  // Matches input_buffer size
#define MAX_COLUMNS 132
#define MAX_ROWS 2  // Two-line input area

int sockfd;
struct libusb_device_handle *keyboard;
uint8_t endpoint_address;
pthread_t network_thread, cursor_thread;
void *network_thread_f(void *);
void *cursor_blink_thread(void *);

int cursor_row = 43, cursor_col = 2;
char input_buffer[BUFFER_SIZE] = {0};
int input_length = 0;

char keycode_to_ascii(uint8_t keycode, uint8_t modifiers)
{
    // Corrected keymap based on USB HID scan codes
    static const char keymap[] = {
        0, 0, 0, 0,                                       // Keycodes 0-3 (unused)
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', // 4-13
        'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', // 14-23
        'u', 'v', 'w', 'x', 'y', 'z', '1', '2', '3', '4', // 24-33
        '5', '6', '7', '8', '9', '0', '-', '=', '[', ' ', // 34-43
        ' ', '-', '=', '[', ']', '\\', '/', ' '          // 44-50 (Space at 50)
    };

    static const char shift_keymap[] = {
        0, 0, 0, 0,
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', // 4-13
        'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', // 14-23
        'U', 'V', 'W', 'X', 'Y', 'Z', '!', '@', '#', '$', // 24-33
        '%', '^', '&', '*', '(', ')', '_', '+', '{', ' ', // 34-43
        '|', '_', '+', '{', '}', '|', '?', ' '            // 44-50 (Space at 50)
    };

    // Check if the keycode is in the valid range (4-50)
    if (keycode >= 4 && keycode <= 50)
    {
        // Return shifted or unshifted character based on modifiers
        return (modifiers & (USB_LSHIFT | USB_RSHIFT)) ? shift_keymap[keycode] : keymap[keycode];
    }

    // Handle special keys like Backspace, Tab, Space, Enter, etc.
    switch (keycode)
    {
    case 0x2A:
        return '\b'; // Backspace
    case 0x28:
        return '\n'; // Enter
    case 0x2B:
        return '\t';
    case 0x2C:
        return ' '; // Space
    // Handle special characters when Shift is toggled
    case 0x34:
        return (modifiers & (USB_LSHIFT | USB_RSHIFT)) ? '"' : '\''; // ' -> "
    case 0x35:
        return (modifiers & (USB_LSHIFT | USB_RSHIFT)) ? '~' : '`'; // ` -> ~
    case 0x36:
        return (modifiers & (USB_LSHIFT | USB_RSHIFT)) ? '<' : ','; // , -> <
    case 0x37:
        return (modifiers & (USB_LSHIFT | USB_RSHIFT)) ? '>' : '.'; // . -> >
    case 0x38:
        return (modifiers & (USB_LSHIFT | USB_RSHIFT)) ? '?' : '/'; // / -> ?
    case 0x33:
        return (modifiers & (USB_LSHIFT | USB_RSHIFT)) ? ':' : ';'; // ; -> :
    default:
        // Log the unknown keycode for debugging
        printf("Unknown keycode: 0x%X\n", keycode);
        return 0;
    }
}





void *network_thread_f(void *ignored)
{
    char recvBuf[BUFFER_SIZE];
    int n;
    while ((n = read(sockfd, recvBuf, BUFFER_SIZE - 1)) > 0)
    {
        recvBuf[n] = '\0';
        display_received_message(recvBuf);
    }
    return NULL;
}

// void *cursor_blink_thread(void *arg)
// {
//     char *input_buffer = (char *)arg;  // Correctly cast void* to char*
//     static int prev_row = 23, prev_col = 2;  // 🔹 Keep track of previous position
//     int row = 23, col = 2;
//     int blink_state = 1;

//     while (1)
//     {
//         // 🔹 Restore character at old cursor position
//         if (prev_col >= 2) {
//             fbputchar(input_buffer[prev_col - 2] ? input_buffer[prev_col - 2] : ' ', prev_row, prev_col);
//         }

//         // 🔹 Toggle cursor visibility
//         if (blink_state)
//         {
//             fbputchar('_', row, col);
//         }
//         else
//         {
//             fbputchar(' ', row, col);
//         }
        
//         blink_state = !blink_state;

//         // 🔹 Update previous position
//         prev_row = row;
//         prev_col = col;

//         usleep(500000); // 500ms blink interval
//     }

//     return NULL;
// }


int main() {
    struct sockaddr_in serv_addr;
    struct usb_keyboard_packet packet;
    int transferred;

    fbopen();
    fbclear();

    fbputs("> ", 43, 0);
    
    keyboard = openkeyboard(&endpoint_address);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr);
    connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    pthread_create(&network_thread, NULL, network_thread_f, NULL);
    pthread_detach(network_thread);

    for (;;) {
        libusb_interrupt_transfer(keyboard, endpoint_address, (unsigned char *)&packet, sizeof(packet), &transferred, 0);
        if (transferred == sizeof(packet)) {
            char c = keycode_to_ascii(packet.keycode[0], packet.modifiers);
            
            // Handle standard character input
            if (c && input_length < BUFFER_SIZE - 1) {
                handle_input_char(c);
            }

            // Handle backspace
            if (packet.keycode[0] == 0x2A) {  
                if (input_length > 0) {
                    input_length--;
                    cursor_col--;
                    if (cursor_col < 2 && cursor_row > 43) {
                        cursor_row--;
                        cursor_col = MAX_COLUMNS - 1;
                    }
                    fbputchar(' ', cursor_row, cursor_col);
                }
            }

            // Handle Enter key (0x28)
            if (packet.keycode[0] == 0x28) {  
                send(sockfd, input_buffer, input_length, 0);
                display_received_message(input_buffer);
                reset_input();
                fbputs("> ", 43, 0);
            }

            // Handle Left Arrow (0x50)
            if (packet.keycode[0] == 0x50 && cursor_col > 2) {  
                cursor_col--;
                draw_cursor();
            }

            // Handle Right Arrow (0x4F)
            if (packet.keycode[0] == 0x4F && cursor_col < input_length + 2) {  
                cursor_col++;
                draw_cursor();
            }

            // Handle Tab Key (0x2B)
            if (packet.keycode[0] == 0x2B) {  
                int spaces_to_add = 4;
                if (cursor_col + spaces_to_add >= MAX_COLUMNS && cursor_row == 43) {
                    int remaining_in_row = MAX_COLUMNS - cursor_col;
                    for (int i = 0; i < remaining_in_row; i++) {
                        fbputchar(' ', cursor_row, cursor_col++);
                    }
                    cursor_col = 2;
                    cursor_row = 44;
                    spaces_to_add -= remaining_in_row;
                }
                for (int i = 0; i < spaces_to_add && cursor_col < MAX_COLUMNS; i++) {
                    fbputchar(' ', cursor_row, cursor_col++);
                }
                draw_cursor();
            }

            draw_cursor();
        }
    }

    pthread_cancel(network_thread);
    pthread_join(network_thread, NULL);
    return 0;
}
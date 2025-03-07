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

char keycode_to_ascii(uint8_t keycode, uint8_t modifiers)
{
    // Corrected keymap based on USB HID scan codes
    static const char keymap[] = {
        0, 0, 0, 0,                                       // Keycodes 0-3 (unused)
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', // 4-13
        'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', // 14-23
        'u', 'v', 'w', 'x', 'y', 'z', '1', '2', '3', '4', // 24-33
        '5', '6', '7', '8', '9', '0', '-', '=', '[', '\t', // 34-43
        ' ', '-', '=', '[', ']', '\\', '/', ' '          // 44-50 (Space at 50)
    };

    static const char shift_keymap[] = {
        0, 0, 0, 0,
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', // 4-13
        'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', // 14-23
        'U', 'V', 'W', 'X', 'Y', 'Z', '!', '@', '#', '$', // 24-33
        '%', '^', '&', '*', '(', ')', '_', '+', '{', '\t', // 34-43
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
    case 0x2b:
        return '\t'; // Tab
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



//////////////// working code/////////////////
// char keycode_to_ascii(uint8_t keycode, uint8_t modifiers)
// {
//     // Properly indexed keymaps for standard and shifted keys
//     static const char keymap[] = {
//         0, 0, 0, 0,                                       // Keycodes 0-3 (unused)
//         'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', // 4-13
//         'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', // 14-23
//         'u', 'v', 'w', 'x', 'y', 'z', '1', '2', '3', '4', // 24-33
//         '5', '6', '7', '8', '9', '0', '-', '=', '[', ']', // 34-43
//         '\\', ';', '\'', '`', ',', '.', '/', ' '          // 44-50 (Space at 50)
//     };

//     static const char shift_keymap[] = {
//         0, 0, 0, 0,
//         'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', // 4-13
//         'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', // 14-23
//         'U', 'V', 'W', 'X', 'Y', 'Z', '!', '@', '#', '$', // 24-33
//         '%', '^', '&', '*', '(', ')', '_', '+', '{', '}', // 34-43
//         '|', ':', '"', '~', '<', '>', '?', ' '            // 44-50 (Space at 50)
//     };

//     if (keycode >= 4 && keycode <= 50)
//     {
//         return (modifiers & (USB_LSHIFT | USB_RSHIFT)) ? shift_keymap[keycode] : keymap[keycode];
//     }

//     // Handle special keys separately
//     switch (keycode)
//     {
//     case 0x2B:
//         return '\t'; // Tab
//     case 0x2C:
//         return ' '; // Space
//     case 0x28:
//         return '\n'; // Enter
//     case 0x2A:
//         return '\b'; // Backspace
//     default:
//         return 0;
//     }
// }

// char keycode_to_ascii(uint8_t keycode, uint8_t modifiers)
// {
//     // Standard QWERTY mapping based on USB HID keycodes
//     const char keymap[] = {
//         0, 0, 0, 0, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
//         'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
//         '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '\n', '\b', '\t', ' ', '-', '=',
//         '[', ']', '\\', ';', '\'', '`', ',', '.', '/'};

//     const char shift_keymap[] = {
//         0, 0, 0, 0, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
//         'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
//         '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '\n', '\b', '\t', ' ', '_', '+',
//         '{', '}', '|', ':', '"', '~', '<', '>', '?'};

//     // Ensure keycode is within bounds
//     if (keycode < 4 || keycode > 53)
//         return 0;

//     // Use shift map if Shift key is held
//     return (modifiers & (USB_LSHIFT | USB_RSHIFT)) ? shift_keymap[keycode - 4] : keymap[keycode - 4];
// }

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

void *cursor_blink_thread(void *arg)
{
    int blink_state = 1;
    while (1)
    {
        if (blink_state)
        {
            draw_cursor(23, 2);
        }
        else
        {
            fbputchar(' ', 23, 2);
        }
        blink_state = !blink_state;
        usleep(500000); // 500ms
    }
    return NULL;
}

int main()
{
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

    for (;;)
    {
        libusb_interrupt_transfer(keyboard, endpoint_address, (unsigned char *)&packet, sizeof(packet), &transferred, 0);
        if (transferred == sizeof(packet))
        {
            char c = keycode_to_ascii(packet.keycode[0], packet.modifiers);
            if (c && input_col - 2 < BUFFER_SIZE - 1)
            { // 🔹 Buffer Protection
                fbputchar(c, input_row, input_col);
                input_buffer[input_col - 2] = c;
                input_col++;
            }
            if (packet.keycode[0] == 0x2A && input_col > 2)
            { // Backspace
                input_col--;
                fbputchar(' ', input_row, input_col);
                input_buffer[input_col - 2] = '\0';
            }
            if (packet.keycode[0] == 0x50 && input_col > 2)
            { // Left Arrow
                input_col--;
            }
            if (packet.keycode[0] == 0x4F && input_col < 64 && input_buffer[input_col - 2])
            { // Right Arrow
                input_col++;
            }
            if (packet.keycode[0] == 0x28)
            { // Enter
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
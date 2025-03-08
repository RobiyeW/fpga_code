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
        '5', '6', '7', '8', '9', '0', '-', '=', '[', ' ', // 34-43
        ' ', '-', '=', '[', ']', '\\', '/', ' '          // 44-50 (Space at 50)
    };

    static const char shift_keymap[] = {
        0, 0, 0, 0,
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', // 4-13
        'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', // 14-23
        'U', 'V', 'W', 'X', 'Y', 'Z', '!', '@', '#', '$', // 24-33
        '%', '^', '&', '*', '(', ')', '_', '+', '{', ' ', // 34-43
        ' ', '_', '+', '{', '}', '|', '?', ' '            // 44-50 (Space at 50)
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

int main()
{
    int col;
    struct sockaddr_in serv_addr;
    struct usb_keyboard_packet packet;
    int transferred, input_col = 2, input_row = 43;
    char input_buffer[BUFFER_SIZE] = {0};


    fbopen();
    fbclear();

    for (col = 0; col < 132; col++) {
        fbputchar('*', 42, col);
    }

    fbputs("> ", 43, 0);

    keyboard = openkeyboard(&endpoint_address);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr);
    connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    pthread_create(&network_thread, NULL, network_thread_f, NULL);
    pthread_detach(network_thread);

    for (;;)
    {
        libusb_interrupt_transfer(keyboard, endpoint_address, (unsigned char *)&packet, sizeof(packet), &transferred, 0);
        if (transferred == sizeof(packet))
        {
            char c = keycode_to_ascii(packet.keycode[0], packet.modifiers);
            if (c && input_col - 2 < BUFFER_SIZE - 1)
            { // 🔹 Ensure character is stored BEFORE moving cursor
                input_buffer[input_col - 2] = c;  
                fbputchar(c, input_row, input_col);
                input_col++;
                printf("%s\n", input_buffer);
                draw_cursor(input_row, input_col, input_buffer);  // 🔹 Update cursor immediately
            }
            if (c && input_col - 2 >= BUFFER_SIZE - 1)
            { // 🔹 Ensure character is stored BEFORE moving cursor
                input_buffer[input_col - 2] = c;  
                fbputchar(c, input_row+1, input_col);
                input_col++;
                printf("%s\n", input_buffer);
                draw_cursor(input_row+1, input_col, input_buffer);  // 🔹 Update cursor immediately
            }

            if (c && input_col < 128) {  
                // If typing in row 43 and reaching column 127, move to row 44
                if (input_row == 43 && input_col == 127) {
                    input_row = 44;
                    input_col = 0;
                }
            
                // Store character in buffer correctly
                int buffer_index = (input_row == 43) ? input_col - 2 : (input_col - 2 + 128);
                input_buffer[buffer_index] = c;
            
                // Print character on screen
                fbputchar(c, input_row, input_col);
                input_col++;
            
                // Move cursor after character
                draw_cursor(input_row, input_col, input_buffer);
            }

            
            if ((packet.keycode[0] == 0x2A || packet.keycode[0] == 0x42) && input_col > 2)
            {
                input_col--;
                fbputchar(' ', input_row, input_col);  // Clear character from framebuffer
                input_buffer[input_col - 2] = '\0';   // Remove from buffer safely
            }
            
            if ((packet.keycode[0] == 0x2B || packet.keycode[0] == 0x43) && input_col < 128)
            { // Tab (0x43) - Moves cursor forward 4 spaces
                for (int i = 0; i < 4; i++)
                {
                    fbputchar(' ', input_row, input_col);
                    input_buffer[input_col - 2] = ''; 
                    input_col++;
                }
            }
    
            // Handle Left Arrow Key (0x50)
            if (packet.keycode[0] == 0x50) {  
                if (input_row == 44 && input_col == 0) {  
                    // Move from start of row 44 to end of row 43
                    input_row = 43;
                    input_col = 127;
                } else if (input_row == 44 && input_col > 0) {  
                    // Move left within row 44
                    input_col--;
                } else if (input_row == 43 && input_col > 2) {  
                    // Move left within row 43 (preventing backtracking past `> `)
                    input_col--;
                }
                draw_cursor(input_row, input_col, input_buffer);
            }

            // Handle Right Arrow Key (0x4F)
            if (packet.keycode[0] == 0x4F) {  
                if (input_row == 43 && input_col == 127) {  
                    // Move from end of row 43 to start of row 44
                    input_row = 44;
                    input_col = 0;
                } else if (input_row == 43 && input_col < 127) {  
                    // Move right within row 43
                    input_col++;
                } else if (input_row == 44 && input_col < 127) {  
                    // Move right within row 44
                    input_col++;
                }
                draw_cursor(input_row, input_col, input_buffer);
            }


            
            if (packet.keycode[0] == 0x28) { // Enter key
                input_buffer[input_col - 3] = '\0';  // ✅ Ensure cursor is removed before sending
                send(sockfd, input_buffer, strlen(input_buffer), 0);
                //display_received_message(input_buffer);
                memset(input_buffer, 0, sizeof(input_buffer));
                fbclear_input_area();
                fbputs("> ", 43, 0);
                input_col = 2;
            }
            
            usleep(10000); // 🔹 Small delay to ensure rendering catches up
            draw_cursor(input_row, input_col, input_buffer);
            
        }
    }

    pthread_cancel(network_thread);
    pthread_join(network_thread, NULL);
    return 0;
}
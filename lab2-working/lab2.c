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

int main()
{
    int col;
    struct sockaddr_in serv_addr;
    struct usb_keyboard_packet packet;
    int transferred, input_col = 2, input_row = 43;
    char input_buffer[BUFFER_SIZE] = {0};

    // for (col = 0; col < 132; col++) {
    //     fbputchar('*', 42, col);
    // }

    fbopen();
    fbclear();

    for (col = 0; col < 130; col++)
    {
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
            if (c && strlen(input_buffer) < BUFFER_SIZE - 1)
            { 
                // Store character in buffer
                input_buffer[strlen(input_buffer)] = c;
                
                // Print character on screen
                fbputchar(c, input_row, input_col);
                input_col++;

                // Wrap to next line when reaching column limit (assume 132 columns)
                if (input_col >= 132)
                {
                    input_col = 0; // Reset column to beginning
                    input_row = 44; // Move cursor to the second row
                }

                draw_cursor(input_row, input_col);
            }

            // Handle backspace (0x2A)
            if (packet.keycode[0] == 0x2A && strlen(input_buffer) > 0)
            {
                input_col--;

                if (input_col < 0)  // Move cursor back to previous row if needed
                {
                    input_col = 132; // Move to last column of previous row
                    input_row = 43;
                }

                fbputchar(' ', input_row, input_col); // Clear character visually
                input_buffer[strlen(input_buffer) - 1] = '\0'; // Remove from buffer
                draw_cursor(input_row, input_col);
            }
            
            if ((packet.keycode[0] == 0x2B || packet.keycode[0] == 0x43)) { // Tab Key
                int spaces_to_add = 4;
            
                // If tab moves beyond column 132 in row 43, move to row 44
                if (input_col + spaces_to_add >= 132 && input_row == 43) {
                    int remaining_in_row = 132 - input_col; // Fill remaining spaces
                    for (int i = 0; i < remaining_in_row; i++) {
                        fbputchar(' ', input_row, input_col);
                        input_col++;
                    }
                    input_col = 0;
                    input_row = 44;
                    spaces_to_add -= remaining_in_row; // Reduce remaining spaces
                }
            
                // Apply remaining tab spaces (ensure we don't exceed column 132 in row 44)
                for (int i = 0; i < spaces_to_add && input_col < 132; i++) {
                    fbputchar(' ', input_row, input_col);
                    input_col++;
                }
            
                draw_cursor(input_row, input_col);
            }
            
    
            // Left Arrow Key (0x50)
            if (packet.keycode[0] == 0x50) {
                if (input_col > 0) {  
                    input_col--;  // Move left within the row
                } 
                else if (input_col == 0 && input_row == 44) {  
                    input_row = 43;  // Move up to row 43
                    input_col = 131; // Move to last column of row 43
                }

                fbputchar(input_buffer[input_col], input_row, input_col); // Restore character
                draw_cursor(input_row, input_col); // Update cursor
            }

            // Right Arrow Key (0x4F)
            if (packet.keycode[0] == 0x4F) { 
                if (input_col < 131) {  
                    input_col++;  // Move right within the row
                } 
                else if (input_col == 131 && input_row == 43) {  
                    input_row = 44;  // Move down to row 44
                    input_col = 0;   // Start at column 0
                }

                fbputchar(input_buffer[input_col], input_row, input_col); // Restore character
                draw_cursor(input_row, input_col); // Update cursor
            }

            
            // Handle Enter (0x28)
            if (packet.keycode[0] == 0x28) {  
                char message_to_send[BUFFER_SIZE] = {0};  // Buffer to store the message

                if (input_row == 43) {  
                    // Send only row 43's content
                    strncpy(message_to_send, input_buffer, 132);  
                } 
                else if (input_row == 44) {  
                    // Send row 43 followed by row 44's content
                    snprintf(message_to_send, BUFFER_SIZE, "%s%s", input_buffer, &input_buffer[132]);  
                }

                send(sockfd, message_to_send, strlen(message_to_send), 0);  // Send to server
                display_received_message(message_to_send);  

                // Clear input area for both rows 43 and 44
                memset(input_buffer, 0, sizeof(input_buffer));
                fbclear_input_area();
                
                // Redraw the user input prompt in row 43
                fbputs("> ", 43, 0);

                // Reset cursor and input tracking
                input_col = 0;
                input_row = 43;
            }


            
            usleep(10000); // 🔹 Small delay to ensure rendering catches up
            draw_cursor(input_row, input_col);
            
        }
    }

    pthread_cancel(network_thread);
    pthread_join(network_thread, NULL);
    return 0;
}
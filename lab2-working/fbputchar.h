#ifndef _FBPUTCHAR_H
#  define _FBPUTCHAR_H

#define FBOPEN_DEV -1          /* Couldn't open the device */
#define FBOPEN_FSCREENINFO -2  /* Couldn't read the fixed info */
#define FBOPEN_VSCREENINFO -3  /* Couldn't read the variable info */
#define FBOPEN_MMAP -4         /* Couldn't mmap the framebuffer memory */
#define FBOPEN_BPP -5          /* Unexpected bits-per-pixel */

extern int fbopen(void);
extern void fbputchar(char, int, int);
extern void fbputs(const char *, int, int);
extern void fbclear(void);
extern void fbclear_input_area(void);
extern void display_received_message(const char *);
extern void scroll_text_up(void);
extern void draw_cursor(int, int);  // 🔹 Remove `input_buffer` argument
extern void store_input_char(int, char);  // 🔹 Function to store input in `input_buffer`
extern void clear_input_buffer(void);  // 🔹 Function to clear the `input_buffer`
extern void get_input_buffer(char *, int);


#endif

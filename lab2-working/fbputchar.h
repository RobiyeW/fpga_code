#ifndef _FBPUTCHAR_H
#  define _FBPUTCHAR_H

#define FBOPEN_DEV -1          
#define FBOPEN_FSCREENINFO -2  
#define FBOPEN_VSCREENINFO -3  
#define FBOPEN_MMAP -4         
#define FBOPEN_BPP -5          

extern int fbopen(void);
extern void fbputchar(char, int, int);
extern void fbputs(const char *, int, int);
extern void fbclear(void);
extern void fbclear_input_area(void);
extern void display_received_message(const char *);
extern void scroll_text_up(void);
extern void draw_cursor(void);
extern void handle_input_char(char);
extern void reset_input(void);

#endif

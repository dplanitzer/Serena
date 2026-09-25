//
//  flowterm.h
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/22/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _FLOWTERM_H
#define _FLOWTERM_H 1

#include <stdio.h>

// Event types
#define FT_EVT_EOF              0
#define FT_EVT_CHAR             1
#define FT_EVT_MOUSE_UP         2
#define FT_EVT_MOUSE_DOWN       3
#define FT_EVT_MOUSE_DRAG       4
#define FT_EVT_MOUSE_MOVE       5
#define FT_EVT_MOUSE_WHEEL      6
#define FT_EVT_REPORT           7
#define FT_EVT_INVALID_REPORT   8


// Event masks
#define FT_MSK_CHAR             (1u << (unsigned int)FT_EVT_CHAR)
#define FT_MSK_MOUSE_UP         (1u << (unsigned int)FT_EVT_MOUSE_UP)
#define FT_MSK_MOUSE_DOWN       (1u << (unsigned int)FT_EVT_MOUSE_DOWN)
#define FT_MSK_MOUSE_DRAG       (1u << (unsigned int)FT_EVT_MOUSE_DRAG)
#define FT_MSK_MOUSE_MOVE       (1u << (unsigned int)FT_EVT_MOUSE_MOVE)
#define FT_MSK_REPORT           (1u << (unsigned int)FT_EVT_REPORT)
#define FT_MSK_INVALID_REPORT   (1u << (unsigned int)FT_EVT_INVALID_REPORT)

#define FT_ANY_CHAR             (FT_MSK_CHAR)
#define FT_ANY_MOUSE            (FT_MSK_MOUSE_UP | FT_MSK_MOUSE_DOWN | FT_MSK_MOUSE_DRAG | FT_MSK_MOUSE_MOVE)
#define FT_ANY_REPORT           (FT_MSK_REPORT | FT_MSK_INVALID_REPORT)
#define FT_ANY                  (FT_ANY_CHAR | FT_ANY_MOUSE | FT_ANY_REPORT)


// Max parameters for a terminal report event
#define FT_MAX_REPORT_PARAMS    8


// ft_getevent() / ft_getchar() flags
#define FT_NONBLOCKING  1
#define FT_PEEK         2


// ft_mousecntl() mask
#define FT_ENABLED      1
#define FT_MOTION       2


struct ft_char_data {
    unsigned int	unicode;
};

struct ft_mouse_data {
    int	x;
	int	y;
	int button_number;
    int modifiers;
};

struct ft_report_data {
    char            first_char;
    char            last_char;
    unsigned short  param[FT_MAX_REPORT_PARAMS];
};

typedef struct ft_event {
    int  type;
	union {
        struct ft_char_data		character;
        struct ft_mouse_data	mouse;
        struct ft_report_data   report;
	}   data;
} ft_event_t;


// Mouse button numbers
#define FT_MOUSE_BUTTON_LEFT    0
#define FT_MOUSE_BUTTON_MIDDLE  1
#define FT_MOUSE_BUTTON_RIGHT   2
#define FT_MOUSE_WHEEL_UP       64
#define FT_MOUSE_WHEEL_DOWN     65
#define FT_MOUSE_WHEEL_LEFT     66
#define FT_MOUSE_WHEEL_RIGHT    67


// Mouse modifier flags
#define FT_MODIFIER_SHIFT   4
#define FT_MODIFIER_ALT     8
#define FT_MODIFIER_CTRL    16
#define FT_MODIFIER_MASK    (FT_MODIFIER_SHIFT | FT_MODIFIER_ALT | FT_MODIFIER_CTRL)


// Unicode PUA code points for function keys
#define FT_CHAR_BACKSPACE       0x08
#define FT_CHAR_TAB             0x09
#define FT_CHAR_LF              0x0a
#define FT_CHAR_CR              0x10
#define FT_CHAR_ESCAPE          0x1b
#define FT_CHAR_CURSOR_UP       0xf700
#define FT_CHAR_CURSOR_DOWN     0xf701
#define FT_CHAR_CURSOR_LEFT     0xf702
#define FT_CHAR_CURSOR_RIGHT    0xf703
#define FT_CHAR_FKEY_F1         0xf704
#define FT_CHAR_FKEY_F2         0xf705
#define FT_CHAR_FKEY_F3         0xf706
#define FT_CHAR_FKEY_F4         0xf707
#define FT_CHAR_FKEY_F5         0xf708
#define FT_CHAR_FKEY_F6         0xf709
#define FT_CHAR_FKEY_F7         0xf70a
#define FT_CHAR_FKEY_F8         0xf70b
#define FT_CHAR_FKEY_F9         0xf70c
#define FT_CHAR_FKEY_F10        0xf70d
#define FT_CHAR_FKEY_F11        0xf70e
#define FT_CHAR_FKEY_F12        0xf70f
#define FT_CHAR_FKEY_F13        0xf710
#define FT_CHAR_FKEY_F14        0xf711
#define FT_CHAR_FKEY_F15        0xf712
#define FT_CHAR_FKEY_F16        0xf713
#define FT_CHAR_FKEY_F17        0xf714
#define FT_CHAR_FKEY_F18        0xf715
#define FT_CHAR_FKEY_F19        0xf716
#define FT_CHAR_FKEY_F20        0xf717
#define FT_CHAR_INSERT          0xf727
#define FT_CHAR_DELETE          0xf728  // Delete Forward
#define FT_CHAR_HOME            0xf729
#define FT_CHAR_BEGIN           0xf72a
#define FT_CHAR_END             0xf72b
#define FT_CHAR_PAGE_UP         0xf72c
#define FT_CHAR_PAGE_DOWN       0xf72d


// Terminal status codes
#define FT_OK   0


// Cursor control options
#define FT_OFF  0
#define FT_ON   1


// Ignore text style
#define FT_IGNORE           0

// Reset text style and colors back to plain and default
#define FT_RESET            0x1

// Add a text style
#define FT_BOLD             0x2 
#define FT_DIM              0x4
#define FT_ITALIC           0x8
#define FT_UNDERLINE        0x10
#define FT_BLINK            0x20
#define FT_INVERSE          0x40
#define FT_HIDDEN           0x80
#define FT_STRIKETHROUGH    0x100

// Remove a text style
#define FT_RESET_BOLD_DIM       0x200 
#define FT_RESET_ITALIC         0x800
#define FT_RESET_UNDERLINE      0x1000
#define FT_RESET_BLINK          0x2000
#define FT_RESET_INVERSE        0x4000
#define FT_RESET_HIDDEN         0x8000
#define FT_RESET_STRIKETHROUGH  0x10000

typedef unsigned int ft_textstyle_t;


// Fixed ANSI color codes
#define FT_ANSI_BLACK   0
#define FT_ANSI_RED     1
#define FT_ANSI_GREEN   2
#define FT_ANSI_YELLOW  3
#define FT_ANSI_BLUE    4
#define FT_ANSI_MAGENTA 5
#define FT_ANSI_CYAN    6
#define FT_ANSI_WHITE   7
#define FT_ANSI_DEFAULT 9   // Reset foreground/background color to the default color value


// Color models
#define FT_COLOR_MODEL_ANSI 0


// Terminal color description
typedef struct ft_color {
    int     model;
    union {
        int     ansi;
    }       color;
} ft_color_t;


// Fixed ANSI colors
extern const ft_color_t ft_ansi_black;
extern const ft_color_t ft_ansi_red;
extern const ft_color_t ft_ansi_green;
extern const ft_color_t ft_ansi_yellow;
extern const ft_color_t ft_ansi_blue;
extern const ft_color_t ft_ansi_magenta;
extern const ft_color_t ft_ansi_cyan;
extern const ft_color_t ft_ansi_white;
extern const ft_color_t ft_ansi_default;    // Reset foreground/background color to the default color value


// Structure used to define the characters that ft_drawrect() should use to draw
// a rectangle outline.
typedef struct rectstyle {
    unsigned int top_left;
    unsigned int top;
    unsigned int top_right;
    unsigned int right;
    unsigned int bottom_right;
    unsigned int bottom;
    unsigned int bottom_left;
    unsigned int left;
} ft_rectstyle;


//
// Note the flowterm takes control of the provided input/output stream. An
// important implication of this is that you should _not_ call fd_cntl() on those
// streams to change their configuration/state. Call the corresponding flowterm
// functions instead so that flowterm can manage configuration and state changes
// correctly.
//
// Concurrency: please note that flowterm assumes that all code that wants to
// receive events or that uses any of the output related functions run on the
// same vcpu.
//
// Buffering:
// * Terminal input: not buffered and turns off buffering on the provided input
//                   stream.
// * Terminal output: does not changing buffering on the provided stream. It is
//                    your responsibility to configure the buffering mode as
//                    needed before or after you pass the stream to ft_settermout()
//                    or ft_init().
//
// Coordinates: all coordinates are 1-based.
//


// Initializes flowterm. Call this function before you call any other flowterm
// function. Pass 0 for 'flags' for now. 
extern int ft_init(unsigned int flags);

// Frees all resources used by flowterm and resets the terminal back to its
// default configuration. Call this function before exiting your app.
extern void ft_cleanup(void);


// Input stream from the terminal
extern FILE* _Nonnull   termin;

// Output stream to the terminal
extern FILE* _Nonnull   termout;


// Sets the terminal input to 'stream' and returns 0 on success. Returns -1 if
// the provided stream is not compatible with flowterm. The default terminal
// input stream is stdin. Note that the provided stream should be backed by a
// file descriptor.
extern int ft_settermin(FILE* _Nonnull stream);

// Sets the terminal output to 'stream' and returns 0 on success. Returns -1 if
// the provided stream is not compatible with flowterm. The default terminal
// output stream is stdout.
extern int ft_settermout(FILE* _Nonnull stream);

// Returns a value > 0 if terminal output is actually connected to a terminal
// and // a value == 0 if the output is connected to something else like a file
// or pipe.
extern int ft_isterm(void);

// Drain all buffered events from the terminal input.
extern void ft_drain(void);

// Flush all buffered output to the terminal.
#define ft_flush() \
fflush(termout)


// Returns the status of the terminal. FT_OK is returned if the connection to
// the terminal works and there is actually a terminal on the other side.
// Otherwise a suitable code is returned. A code < 0 indicates a connection
// problem (see errno for more detailed error info) and a code > FT_OK indicates
// a problem with the terminal itself.
extern int ft_status(void);


// Configures the generation of mouse events. Mouse event generation may be
// turned on or off. It is turned off by default. Turning mouse event generation
// on will cause the console to show a mouse cursor. Additionally allows you to
// control whether mouse motion events should be generated or not. Mouse motion
// events are turned off by default and only mouse clicks are registered. 
extern unsigned int ft_mousecntl(unsigned int mask);

// Blocks the caller until a character, mouse or terminal report event arrives.
// Specify the FT_NONBLOCKING option if the function should return without
// blocking if no events are available. The function returns -1 and errno is set
// to EAGAIN in this case.
// 'mask' controls which kinds of events the function will wait for. The event
// record is returned in 'evt'.
// AN EOF event is returned if either the current input stream is not a valid
// or teh underlying stream read operation has encountered an EOF condition. 
extern int ft_getevent(unsigned int mask, unsigned int flags, ft_event_t* _Nonnull evt);

// Returns the next available character. Blocks the caller until a character is
// available and FT_NONBLOCKING is not specified. If FT_NONBLOCKING is specified
// and no character is available then 0 is returned. Returns EOF if an error
// has occurred. Note that the caller is blocked until a character event is
// available. If e.g. mouse events are enabled and a mouse event is pending then
// this mouse event is internally discarded and ft_getchar() continues to wait
// until a character event becomes available.
// This function returns EOF and sets errno to 0 if it encounters an EOF condition.
extern int ft_getchar(unsigned int flags);


// Returns the current cursor position.
extern void ft_curpos(int* _Nonnull x, int* _Nonnull y);

// Returns the size of the terminal screen.
extern void ft_screensize(int* _Nonnull width, int* _Nonnull height);


// Clears the screen and moves the cursor to the home position (1, 1).
extern void ft_cls(void);


// Saves/restores the current text cursor position, text style, text foreground
// and background colors.
extern void ft_save(void);
extern void ft_restore(void);


// Shows or hides the terminal text cursor. Use FT_ON toshow it and FT_OFF to
// hide it.
extern void ft_cursor(int op);

// Selects insert (FT_ON) or replace mode (FT_OFF). Note that ft_init() selects
// replace mode by default.
extern void ft_insertmode(int op);

// Enables auto-wrap (FT_ON) or disables it (FT_OFF).
extern void ft_autowrap(int op);


// Moves the cursor to the home location, which is (1, 1).
extern void ft_home(void);

// Moves the text cursor to the specified absolute position (x, y). Note that
// cursor coordinates are 1-based.
extern void ft_moveto(int x, int y);

// Moves the text cursor by 'dx' and 'dy' cells. A negative 'dy' moves the
// cursor up and a negative 'dx' moves the cursor to the left. Note that cursor
// coordinates are 1-based.
extern void ft_move(int dx, int dy);


// Applies the text style 'style', the foreground color 'fg' and the background
// color 'bg' to the current text style state of the terminal. If FT_RESET is
// specified then the current text style is first reset back to the defaults.
// After that all non-reset text styles are applied and finally all reset styles
// are applied. A color which is passed as NULL remains unchanged.
extern void ft_style(ft_textstyle_t style, const ft_color_t* _Nullable fg, const ft_color_t* _Nullable bg);

// Convenience macro to reset the text style back to plain with default
// foreground and background colors.
#define ft_resetstyle() \
ft_style(FT_RESET, NULL, NULL)

// Convenience macro to just change the foreground color.
#define ft_fgcolor(clr_ptr) \
ft_style(FT_IGNORE, clr_ptr, NULL)

// Convenience macro to just change the background color.
#define ft_bgcolor(clr_ptr) \
ft_style(FT_IGNORE, NULL, clr_ptr)


// Draws a horizontal line of length 'count' from left to right, starting at the
// current cursor. 'ch' is the character that should be used to draw the line.
// No line is drawn if 'ch' is 0 or 'count' is <= 0.
extern void ft_hline(unsigned int ch, int count);

// Similar to hline(), but draws a vertical line from top to bottom.
extern void ft_vline(unsigned int ch, int count);


// Fills a rectangle with the character 'ch'. The top-left corner of the rectangle
// is at the current cursor position, 'width' is the width and 'height' is the
// height of the rectangle.
extern void ft_fillrect(unsigned int ch, int width, int height);

// Clears a rectangular area by filling it with the ' ' (space) character.
extern void ft_clearrect(int width, int height);


// Draw top and bottom edges with '-', left and right edges with '|' and all
// corners with '-';
extern const ft_rectstyle ft_rectstyle_hflat;

// Draw top and bottom edges with '-', left and right edges with '|' and all
// corners with '|';
extern const ft_rectstyle ft_rectstyle_vflat;

// Draw top and bottom edges with '-', left and right edges with '|' and all
// corners with '+';
extern const ft_rectstyle ft_rectstyle_plus;


// Draws the outline of a rectangle. The top-left corner of the rectangle is
// given by the current cursor position, 'width' is its width and 'height' its
// height. 'style' specifies the drawing style that should be used.
extern void ft_drawrect(const ft_rectstyle* _Nonnull style, int width, int height);


// Writes 'nbytes' from 'buf' to the terminal and returns the number of bytes
// actually written.
#define ft_write(buf, nbytes) \
fwrite(buf, 1, nbytes, termout)

// Writes the string 'str' to the terminal and returns the number of characters
// actually written. Note that this function behaves like fputs() and thus it
// does not implicitly append a newline at the end of the string.
#define ft_puts(str) \
fputs(str, termout)

// Writes the character 'ch' to the terminal.
#define ft_putc(ch) \
putc(ch, termout)

// Writes a formatted C string to the terminal. Supports all printf() format
// specifiers including floating-point related ones.
#define ft_printf(format, ...) \
fprintf(termout, format, __VA_ARGS__)

#define ft_vprintf(format, ap) \
vfprintf(termout, format, ap)

// Writes a formatted C string to the terminal. Only non-floating-point related
// format specifiers are supported.
#define ft_iprintf(format, ...) \
fiprintf(termout, format, __VA_ARGS__)

#define ft_viprintf(format, ap) \
vfiprintf(termout, format, ap)

#endif /* _FLOWTERM_H */

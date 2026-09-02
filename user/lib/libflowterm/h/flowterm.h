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
#define FT_EVT_NULL             0
#define FT_EVT_CHAR             1
#define FT_EVT_MOUSE_UP         2
#define FT_EVT_MOUSE_DOWN       3
#define FT_EVT_MOUSE_DRAG       4
#define FT_EVT_MOUSE_MOVE       5
#define FT_EVT_MOUSE_WHEEL      6
#define FT_EVT_REPORT           7
#define FT_EVT_INVALID_REPORT   8


// Event masks
#define FT_MSK_NULL             (1u << (unsigned int)FT_EVT_NULL)
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
#define FT_ANY                  (FT_MSK_NULL | FT_ANY_CHAR | FT_ANY_MOUSE | FT_ANY_REPORT)


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
// * Terminal output: buffered
//
// Coordinates: all coordinates are 1-based.
//


// Initializes flowterm. Call this function before you call any other flowterm
// function.
extern int ft_init(void);

// Frees all resources used by flowterm and resets the terminal back to its
// default configuration. Call this function before exiting your app.
extern void ft_cleanup(void);


// Set the terminal input to 'stream'. The default terminal input stream is stdin.
// Note that the provided stream has to be backed by a file descriptor.
extern FILE* _Nonnull ft_termin(FILE* _Nonnull stream);

// Set the terminal output to 'stream'. The default terminal output stream is
// stdout.
extern FILE* _Nonnull ft_termout(FILE* _Nonnull stream);

// Drain all buffered events from the terminal input.
extern void ft_drain(void);

// Flush all buffered output to the terminal.
extern void ft_flush(void);


// Returns the status of the terminal. FT_OK is returned if the connection to
// the terminal works and there is actually a terminal on the other side.
// Otherwise a suitable code is returned. A code < 0 indicates a connection
// problem (see errno for more detailed error info) and a code > FT_OK indicates
// a problem with the terminal itself.
extern int ft_status(void);


extern unsigned int ft_mousecntl(unsigned int mask);
extern int ft_getevent(unsigned int mask, unsigned int flags, ft_event_t* _Nonnull evt);

// Returns the next available character. Blocks the caller until a character is
// available and FT_NONBLOCKING is not specified. If FT_NONBLOCKING is specified
// and no character is available then 0 is returned. Returns EOF if an error
// has occurred. Note that the caller is blocked until a character event is
// available. If e.g. mouse events are enabled and a mouse event is pending then
// this mouse event is internally discarded and ft_getchar() continues to wait
// until a character event becomes available.
extern int ft_getchar(unsigned int flags);


// Returns the current cursor position.
extern void ft_curpos(int* _Nonnull x, int* _Nonnull y);

// Returns the size of the terminal screen.
extern void ft_screensize(int* _Nonnull width, int* _Nonnull height);


// Saves/restores the current text cursor position, text style, text foreground
// and background colors.
extern void ft_save(void);
extern void ft_restore(void);


// Configures various text cursor properties. E.g. whether the text cursor is
// on or off.
extern void ft_cursorcntl(unsigned int flags);

// Moves the text cursor to the specified absolute position (x, y). Note that
// cursor coordinates are 1-based.
extern void ft_moveto(int x, int y);

// Moves the text cursor by 'dx' and 'dy' cells. A negative 'dy' moves the
// cursor up and a negative 'dx' moves the cursor to the left. Note that cursor
// coordinates are 1-based.
extern void ft_move(int dx, int dy);

#endif /* _FLOWTERM_H */

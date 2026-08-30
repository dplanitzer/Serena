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

// Initializes flowterm. Call this function before you call any other flowterm
// function.
extern int ft_init(void);

// Frees all resources used by flowterm and resets the terminal back to its
// default configuration. Call this function before exiting your app.
extern void ft_cleanup(void);


// Event types
#define FT_EVT_NULL             0
#define FT_EVT_CHAR             1
#define FT_EVT_MOUSE_DOWN       2
#define FT_EVT_MOUSE_UP         3
#define FT_EVT_SCROLL_WHEEL     4
#define FT_EVT_CURSOR_POSITION  5
#define FT_EVT_SCREEN_SIZE      6

// Event masks
#define FT_MSK_NULL             (1u << (unsigned int)FT_EVT_NULL)
#define FT_MSK_CHAR             (1u << (unsigned int)FT_EVT_CHAR)
#define FT_MSK_MOUSE_DOWN       (1u << (unsigned int)FT_EVT_MOUSE_DOWN)
#define FT_MSK_MOUSE_UP         (1u << (unsigned int)FT_EVT_MOUSE_UP)
#define FT_MSK_SCROLL_WHEEL     (1u << (unsigned int)FT_EVT_SCROLL_WHEEL)
#define FT_MSK_CURSOR_POSITION  (1u << (unsigned int)FT_EVT_CURSOR_POSITION)
#define FT_MSK_SCREEN_SIZE      (1u << (unsigned int)FT_EVT_SCREEN_SIZE)

#define FT_ANY_CHAR             (FT_MSK_CHAR)
#define FT_ANY_MOUSE            (FT_MSK_MOUSE_DOWN | FT_MSK_MOUSE_UP | FT_MSK_SCROLL_WHEEL)
#define FT_ANY                  (FT_MSK_NULL | FT_ANY_CHAR | FT_ANY_MOUSE)


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
	unsigned int buttons;
};

struct ft_scroll_wheel_data {
    int	dx;
	int	dy;
};

struct ft_cursor_data {
    int	x;
	int	y;
};

struct ft_screen_data {
    int	x;
	int	y;
};

typedef struct ft_event {
    int  type;
	union {
        struct ft_char_data			    character;
        struct ft_mouse_data			mouse;
        struct ft_scroll_wheel_data	    scroll_wheel;
        struct ft_cursor_data			cursor;
        struct ft_screen_data			screen;
	}   data;
} ft_event_t;


// Note the flowterm takes control of the provided input/output stream. An
// important implication of this is that you should _not_ call fd_cntl() on those
// streams to change their configuration/state. Call the corresponding flowterm
// functions instead so that flowterm can manage configuration and state changes
// correctly.

extern FILE* _Nonnull ft_termin(FILE* _Nonnull stream);
extern FILE* _Nonnull ft_termout(FILE* _Nonnull stream);

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

#endif /* _FLOWTERM_H */

//
//  __flowterm.h
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/22/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _FLOWTERM_PRIV_H
#define _FLOWTERM_PRIV_H 1

#include <flowterm.h>
#include <errno.h>

// ft_getevent() generates these special event codes for well-known terminal
// reports that are handled by flowterm internally. All other terminal reports
// are mapped to the generic FT_EVT_REPORT event type and are made visible to
// the app.
#define _FT_EVT_CURSOR_POSITION 15
#define _FT_EVT_STATUS          16

#define _FT_MSK_CURSOR_POSITION (1u << (unsigned int)_FT_EVT_CURSOR_POSITION)
#define _FT_MSK_STATUS          (1u << (unsigned int)_FT_EVT_STATUS)


extern int              __ft_termin_fd;
extern FILE* _Nonnull   __ft_termout_fp;


extern void _ft_config_termin(unsigned int flags);

extern void _ft_init_events(void);


#define _FT_ITOA_BUF_SIZE   6       // Longest possible digit sequence + trailing NUL character
#define _FT_ITOA_MAX        65535
#define _FT_ITOA_MIN        0

extern char* _Nonnull __ft_itoa(int val, char* _Nonnull buf);


// Work buffer to construct escape sequences to send them to the terminal
#define _FT_OUTBUF_SIZE 64
extern char __ft_outbuf[_FT_OUTBUF_SIZE];

#endif /* _FLOWTERM_PRIV_H */

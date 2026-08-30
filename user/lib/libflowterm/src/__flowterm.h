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

extern int              __ft_termin_fd;
extern FILE* _Nonnull   __ft_termout_fp;


extern void _ft_config_termin(unsigned int flags);

extern void _ft_init_events(void);

#endif /* _FLOWTERM_PRIV_H */

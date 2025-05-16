//
//  sys/_dirent.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_PRIV_DIRENT_H
#define _SYS_PRIV_DIRENT_H 1

#include <System/_syslimits.h>
#ifdef __KERNEL__
#include <kern/types.h>
#else
#include <sys/types.h>
#endif

typedef struct dirent {
    ino_t   inid;
    char    name[__PATH_COMPONENT_MAX];
} dirent_t;

#endif /* _SYS_PRIV_DIRENT_H */

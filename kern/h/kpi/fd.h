//
//  kpi/fd.h
//  kpi
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_FD_H
#define _KPI_FD_H 1

// Standard descriptors that are open when a process starts. These descriptors
// connect the process to the terminal input and output streams.
#define FD_STDIN    0
#define FD_STDOUT   1
#define FD_STDERR   2


// File (descriptor) flags (fd_flags_t)
#define O_RDONLY    0x0001
#define O_WRONLY    0x0002
#define O_RDWR      (O_RDONLY | O_WRONLY)
#define O_EXCL      0x0004
#define O_TRUNC     0x0008
#define O_APPEND    0x0010
#define O_NONBLOCK  0x0020
#define O_PRSVEXEC  0x0040
#if defined(__KERNEL__)
#define O_EXONLY    0x10000
#define O_USERSPACE 0x20000
#endif

// File flags masks
#define O_ACCMASK   (O_RDONLY | O_WRONLY | O_RDWR)
#define O_MODMASK   (O_APPEND | O_NONBLOCK | O_PRSVEXEC)
#if defined(__KERNEL__)
#define O_USERMASK  (O_ACCMASK | O_MODMASK | O_EXCL | O_TRUNC)
#endif


// fd_setflags() operation modes
#define FD_FOP_REPLACE  1
#define FD_FOP_ADD      2
#define FD_FOP_REMOVE   3


// Descriptor types.
#define FD_TYPE_INVALID     -1
#define FD_TYPE_TERMINAL    0
#define FD_TYPE_INODE       1
#define FD_TYPE_DEVICE      2

#endif /* _KPI_FD_H */

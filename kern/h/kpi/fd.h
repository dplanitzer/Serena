//
//  kpi/fd.h
//  kpi
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_FD_H
#define _KPI_FD_H 1

// File access modes and status bits
#define O_RDONLY    0x0001
#define O_WRONLY    0x0002
#define O_RDWR      (O_RDONLY | O_WRONLY)
#define O_CREAT     0x0004
#define O_EXCL      0x0008
#define O_TRUNC     0x0010
#define O_APPEND    0x0020
#define O_NONBLOCK  0x0040
#define _O_EXONLY   0x0080

// File access mode and status masks
#define O_ACCMODE       (O_RDONLY | O_WRONLY | O_RDWR)
#define O_FILESTATUS    (O_APPEND | O_NONBLOCK)


// fd_setflags() operation modes
#define FD_FOP_REPLACE  1
#define FD_FOP_ADD      2
#define FD_FOP_REMOVE   3


// Descriptor types.
#define FD_TYPE_INVALID     -1
#define FD_TYPE_TERMINAL    0
#define FD_TYPE_INODE       1
#define FD_TYPE_DRIVER      2
#define FD_TYPE_PROCESS     3


#define FD_INFO_BASIC   1

typedef void* fd_info_ref;

typedef struct fd_basic_info {
    int     type;
    int     flags;
} fd_basic_info_t;

#endif /* _KPI_FD_H */

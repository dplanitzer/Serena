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


// Returns the descriptor flags (int)
// int fcntl(int fd, F_GETFD)
#define F_GETFD     0   //XXX obsolete


// Returns file status and access modes
// int fcntl(int fd, F_GETFL)
#define F_GETFL     1   //XXX obsolete

// Sets the file status to the given bits, ignoring any bits outside O_FILESTATUS
// int fcntl(int fd, F_SETFL, int fsbits)
#define F_SETFL     2

// Updates the file status to the given bits, ignoring any bits outside O_FILESTATUS.
// If 'setOrClear' is 0 then the specified bits are cleared; otherwise the specified
// bits are set in the descriptor
// int fcntl(int fd, F_SETFL, int setOrClear, int fsbits)
#define F_UPDTFL    3


// Returns the descriptor type
// int fcntl(int fd, F_GETTYPE)
#define F_GETTYPE   4   //XXX obsolete

#endif /* _KPI_FD_H */

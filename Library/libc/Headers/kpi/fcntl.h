//
//  kpi/fcntl.h
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_FCNTL_H
#define _KPI_FCNTL_H 1

// File access modes and status bits
#define O_RDONLY    0x0001
#define O_WRONLY    0x0002
#define O_RDWR      (O_RDONLY | O_WRONLY)
#define O_APPEND    0x0004
#define O_EXCL      0x0008
#define O_TRUNC     0x0010
#define O_NONBLOCK  0x0020

// File access mode and status masks
#define O_ACCMODE       (O_RDONLY | O_WRONLY | O_RDWR)
#define O_FILESTATUS    (O_APPEND | O_NONBLOCK)


// Descriptor types.
#define SEO_FT_TERMINAL     0
#define SEO_FT_REGULAR      1
#define SEO_FT_DIRECTORY    2
#define SEO_FT_PIPE         3
#define SEO_FT_DRIVER       4
#define SEO_FT_FILESYSTEM   5
#define SEO_FT_PROCESS      6


// Returns the descriptor flags (int)
// int fcntl(int fd, F_GETFD)
#define F_GETFD     0


// Returns file status and access modes
// int fcntl(int fd, F_GETFL)
#define F_GETFL     1

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
#define F_GETTYPE   4

#endif /* _KPI_FCNTL_H */

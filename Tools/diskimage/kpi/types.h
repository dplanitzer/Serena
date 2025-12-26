//
//  kpi/types.h
//  diskimage
//
//  Created by Dietmar Planitzer on 5/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_TYPES_H
#define _KPI_TYPES_H 1

#include <stddef.h>
#include <_time.h>
#include <sys/types.h>
#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

// Process and user related ids
typedef int             pid_t;
typedef unsigned int    uid_t;
typedef unsigned int    gid_t;
typedef unsigned int    id_t;


// Represents a logical block address, count and block size
typedef size_t  blkno_t;
typedef size_t  blkcnt_t;
typedef ssize_t blksize_t;


// Represents a logical sector address and count
typedef size_t  sno_t;
typedef size_t  scnt_t;


// The non-persistent, globally unique ID of a filesystem. This ID does not survive
// a system reboot. Id 0 represents a filesystem that does not exist.
typedef unsigned int    fsid_t;


// The persistent, filesystem unique ID of a filesystem inode. This ID is only
// unique with respect to the filesystem to which it belongs.
#if !defined(_WIN32) && !defined(_WIN64)
typedef size_t          ino_t;
#endif


typedef int             nlink_t;
typedef unsigned int    mode_t;


// Disk sector address based in cylinder, head, sector notation 
typedef struct chs {
    size_t  c;
    size_t  h;
    sno_t   s;
} chs_t;


// Time unit of the scheduler clock which increments monotonically and once per clock interrupt
#if defined(__LLP64__) || defined(__LP64__)
typedef long long   tick_t;
#else
typedef long    tick_t;
#endif


// Function types 
typedef void (*VoidFunc_1)(void*);
typedef void (*VoidFunc_2)(void*, void*);

#endif /* _KPI_TYPES_H */

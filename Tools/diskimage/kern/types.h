//
//  kern/types.h
//  diskimage
//
//  Created by Dietmar Planitzer on 5/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_TYPES_H
#define _KERN_TYPES_H 1

#include <System/_cmndef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
#ifndef __SIZE_WIDTH
#define __SIZE_WIDTH 64
#endif

__CPP_BEGIN

// Floating point types
typedef unsigned short  float16_t;
typedef float           float32_t;
typedef double          float64_t;
typedef struct float96_t  { unsigned int w[3]; }  float96_t;
typedef struct float128_t { unsigned int w[4]; }  float128_t;


// Process and user related ids
typedef int             pid_t;
typedef unsigned int    uid_t;
typedef unsigned int    gid_t;
typedef unsigned int    id_t;


// Type to identify a clock
typedef int clockid_t;


// Represents a logical block address, count and block size
typedef size_t  blkno_t;
typedef size_t  blkcnt_t;
typedef ssize_t blksize_t;


// The non-persistent, globally unique ID of a filesystem. This ID does not survive
// a system reboot. Id 0 represents a filesystem that does not exist.
typedef unsigned int    fsid_t;


// The persistent, filesystem unique ID of a filesystem inode. This ID is only
// unique with respect to the filesystem to which it belongs.
#ifndef _WIN32
typedef size_t          ino_t;
#endif


typedef int             nlink_t;
typedef unsigned int    mode_t;


// Represents a logical sector address and count
typedef size_t  sno_t;
typedef size_t  scnt_t;


// Disk sector address based in cylinder, head, sector notation 
typedef struct chs {
    size_t  c;
    size_t  h;
    sno_t   s;
} chs_t;


typedef int32_t Quantums;             // Time unit of the scheduler clock which increments monotonically and once per quantum interrupt


// Function types 
typedef void (*VoidFunc_1)(void*);
typedef void (*VoidFunc_2)(void*, void*);


typedef uint16_t    FilePermissions;
typedef int8_t      FileType;

__CPP_END

#endif /* _KERN_TYPES_H */

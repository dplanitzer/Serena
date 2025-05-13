//
//  types.h
//  libc
//
//  Created by Dietmar Planitzer on 5/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H 1

#include <System/abi/_dmdef.h>
#include <System/abi/_off.h>
#include <System/abi/_size.h>
#include <System/abi/_ssize.h>
#include <System/_cmndef.h>
#include <System/_time.h>

__CPP_BEGIN

// Represents a logical block address and count
typedef size_t  blkno_t;
typedef blkno_t blkcnt_t;


// Type to identify a clock
typedef int clockid_t;


// Various IDs
typedef int         pid_t;
typedef uint32_t    uid_t;
typedef uint32_t    gid_t;
typedef uint32_t    id_t;


// The non-persistent, globally unique ID of a filesystem. This ID does not survive
// a system reboot. Id 0 represents a filesystem that does not exist.
typedef uint32_t    fsid_t;


// The persistent, filesystem unique ID of a filesystem inode. This ID is only
// unique with respect to the filesystem to which it belongs.
#if defined(__ILP32__)
typedef uint32_t    ino_t;
#elif defined(__LP64__) || defined(__LLP64__)
typedef uint64_t    ino_t;
#else
#error "Unknown data model"
#endif


typedef int             nlink_t;
typedef unsigned int    mode_t;

__CPP_END

#endif /* _SYS_TYPES_H */

//
//  sys/types.h
//  libc
//
//  Created by Dietmar Planitzer on 5/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H 1

#include <machine/abi/_off.h>
#include <machine/abi/_size.h>
#include <machine/abi/_ssize.h>
#include <kern/_cmndef.h>
#include <kpi/_time.h>

__CPP_BEGIN

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
typedef size_t          ino_t;


typedef int             nlink_t;
typedef unsigned int    mode_t;

__CPP_END

#endif /* _SYS_TYPES_H */

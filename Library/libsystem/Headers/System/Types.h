//
//  Types.h
//  libsystem
//
//  Created by Dietmar Planitzer on 10/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_TYPES2_H
#define _SYS_TYPES2_H 1

#include <System/abi/_dmdef.h>
#include <System/abi/_bool.h>
#include <System/abi/_inttypes.h>
#include <System/_null.h>
#include <System/_errno.h>
#include <System/_cmndef.h>
#ifndef __SYSTEM_SHIM__
#include <sys/types.h>
#else
//XXX tmp
#include <System/abi/_off.h>
#include <System/abi/_size.h>
#include <System/abi/_ssize.h>

typedef size_t  blkno_t;
typedef blkno_t blkcnt_t;
typedef ssize_t blksize_t;


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
typedef size_t    ino_t;


typedef int             nlink_t;
typedef unsigned int    mode_t;
#endif

__CPP_BEGIN


// Represents a logical sector address and count
typedef size_t  sno_t;
typedef sno_t   scnt_t;

typedef struct chs {
    size_t  c;
    size_t  h;
    sno_t   s;
} chs_t;


typedef uint16_t    FilePermissions;
typedef int8_t      FileType;


#ifndef OFF_MIN
#define OFF_MIN  __OFF_MIN
#endif
#ifndef OFF_MAX
#define OFF_MAX  __OFF_MAX
#endif
#ifndef OFF_WIDTH
#define OFF_WIDTH __OFF_WIDTH
#endif


#ifndef SSIZE_MIN
#define SSIZE_MIN  __SSIZE_MIN
#endif
#ifndef SSIZE_MAX
#define SSIZE_MAX  __SSIZE_MAX
#endif
#ifndef SSIZE_WIDTH
#define SSIZE_WIDTH __SSIZE_WIDTH
#endif


#ifndef SIZE_MAX
#define SIZE_MAX __SIZE_MAX
#endif
#ifndef SIZE_WIDTH
#define SIZE_WIDTH __SIZE_WIDTH
#endif

__CPP_END

#endif /* _SYS_TYPES2_H */

//
//  Types.h
//  libsystem
//
//  Created by Dietmar Planitzer on 10/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H 1

#include <System/abi/_dmdef.h>
#include <System/abi/_bool.h>
#include <System/abi/_inttypes.h>
#include <System/_null.h>
#include <System/abi/_off.h>
#include <System/abi/_size.h>
#include <System/abi/_ssize.h>
#include <System/_errno.h>
#include <System/_cmndef.h>

__CPP_BEGIN

// The non-persistent, globally unique ID of a published driver catalog entry.
// This ID does not survive a system reboot. Id 0 represents a driver catalog
// entry that does not exist.
typedef uint32_t    DriverCatalogId;

// Means no driver catalog entry
#define kDriverCatalogId_None   0


// Various Kernel API types
typedef int         pid_t;

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


// Represents a logical block address in the range 0..<DiskDriver.blockCount
#if defined(__ILP32__)
typedef uint32_t    LogicalBlockAddress;
#elif defined(__LP64__) || defined(__LLP64__)
typedef uint64_t    LogicalBlockAddress;
#else
#error "Unknown data model"
#endif

// Type to represent the number of blocks on a disk
typedef LogicalBlockAddress LogicalBlockCount;


// Unique disk media ID. A value of 0 indicates that no media is loaded in the
// drive.
typedef uint32_t    MediaId;


// Unique disk drive ID. A value of 0 represents the "does not exist" disk drive.
typedef uint32_t    DiskId;


typedef uint16_t    FilePermissions;
typedef int8_t      FileType;

typedef uint32_t    uid_t;
typedef uint32_t    gid_t;
typedef uint32_t    id_t;

typedef int         nlink_t;


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

#endif /* _SYS_TYPES_H */

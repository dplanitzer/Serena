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
#include <System/abi/_size.h>
#include <System/abi/_ssize.h>
#include <System/_errno.h>
#include <System/_cmndef.h>

__CPP_BEGIN

// Various Kernel API types
typedef int         ProcessId;
typedef int32_t     FilesystemId;

#if defined(__ILP32__)
typedef uint32_t    InodeId;
#elif defined(__LP64__) || defined(__LLP64__)
typedef uint64_t    InodeId;
#else
#error "Unknown data model"
#endif

// Represents a logical block address in the range 0..<DiskDriver.blockCount
typedef uint32_t    LogicalBlockAddress;

// Type to represent the number of blocks on a disk
typedef LogicalBlockAddress LogicalBlockCount;


// Unique disk media ID. A value of 0 indicates that no media is loaded in the
// drive.
typedef uint32_t    MediaId;


typedef uint16_t    FilePermissions;
typedef int8_t      FileType;
typedef int64_t     FileOffset;

typedef uint32_t    UserId;
typedef uint32_t    GroupId;

#define kFileOffset_Min 0ll
#define kFileOffset_Max INT64_MAX


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

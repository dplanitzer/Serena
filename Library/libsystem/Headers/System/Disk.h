//
//  Disk.h
//  libsystem
//
//  Created by Dietmar Planitzer on 10/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_DISK_H
#define _SYS_DISK_H 1

#include <System/Driver.h>

__CPP_BEGIN

// No media/empty drive
#define kMediaId_None   0

// Indicates that the cached disk blocks for the currently loaded disk media
// should be synced
#define kMediaId_Current    (~0)


// Disk/media properties
enum {
    kMediaProperty_IsRemovable = 0x0001,
    kMediaProperty_IsReadOnly = 0x0002,
};


// General information about a disk drive and the currently loaded media
typedef struct DiskInfo {
    MediaId             mediaId;            // ID of the currently loaded media; changes with every media eject and insertion; 0 means no media is loaded
    uint32_t            properties;         // Disk/media properties 
    size_t              blockSize;          // byte size of a single disk block. This is the data portion only without any header information
    LogicalBlockCount   blockCount;         // overall number of addressable blocks on the disk
    size_t              mediaBlockSize;     // physical size of a block on the disk media. Only relevant if you want to display this value to the user
    LogicalBlockCount   mediaBlockCount;    // number of physical blocks on the disk media
} DiskInfo;


// Returns information about a disk drive.
// get_info(DiskInfo* _Nonnull pOutInfo)
#define kDiskCommand_GetInfo  IOResourceCommand(kDriverCommand_SubclassBase + 0)


#if !defined(__KERNEL__)

// Synchronously writes all dirty disk blocks back to disk.
extern void Sync(void);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_DISK_H */

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


// Returns information about a disk drive.
// get_info(DiskInfo* _Nonnull pOutInfo)
#define kDiskCommand_GetInfo  IOResourceCommand(kDriverCommand_SubclassBase + 0)

typedef struct DiskInfo {
    MediaId             mediaId;            // ID of the currently loaded media; changes with every media eject and insertion; 0 means no media is loaded 
    bool                isReadOnly;         // true if the data on the disk is hardware write protected
    char                reserved[3];
    size_t              blockSize;          // byte size of a single disk block. This is the data portion only without any header information
    LogicalBlockCount   blockCount;         // overall number of addressable blocks on the disk
    size_t              mediaBlockSize;     // physical size of a block on the disk media. Only relevant if you want to display this value to the user
    LogicalBlockCount   mediaBlockCount;    // number of physical blocks on the disk media
} DiskInfo;


#if !defined(__KERNEL__)

// Synchronously writes all dirty disk blocks back to disk.
extern void Sync(void);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_DISK_H */

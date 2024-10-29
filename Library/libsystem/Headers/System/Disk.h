//
//  Disk.h
//  libsystem
//
//  Created by Dietmar Planitzer on 10/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_DISK_H
#define _SYS_DISK_H 1

#include <System/IOChannel.h>

__CPP_BEGIN

// No media/empty drive
#define kMediaId_None   0


// Returns information about a disk drive.
#define kIODiskCommand_GetInfo  IOResourceCommand(0)

typedef struct DiskInfo {
    size_t              blockSize;          // byte size of a single disk block. This is the data portion only without any header information
    LogicalBlockCount   blockCount;         // overall number of addressable blocks on the disk
    MediaId             mediaId;            // Id of the currently loaded media; changes with every media eject and insertion; 0 means no media is loaded 
    bool                isReadOnly;         // true if the data on the disk is hardware write protected 
} DiskInfo;

__CPP_END

#endif /* _SYS_DISK_H */

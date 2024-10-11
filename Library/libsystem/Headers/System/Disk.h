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

// Represents a logical block address in the range 0..<DiskDriver.blockCount
typedef uint32_t    LogicalBlockAddress;

// Type to represent the number of blocks on a disk
typedef LogicalBlockAddress LogicalBlockCount;


// Returns information about a disk drive.
#define kIODiskCommand_GetInfo  IOResourceCommand(0)

typedef struct DiskInfo {
    size_t              blockSize;          // byte size of a single disk block. This is the data portion only without any header information
    LogicalBlockCount   blockCount;         // overall number of addressable blocks on the disk
    bool                isReadOnly;         // true if the data on the disk is hardware write protected 
    bool                isMediaLoaded;      // true if a disk is in the drive
} DiskInfo;

__CPP_END

#endif /* _SYS_DISK_H */

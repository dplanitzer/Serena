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
    size_t              sectorSize;         // size of a sector (physical block) stored on the disk media. Only relevant if you want to display this value to the user or format a disk
    LogicalBlockCount   sectorCount;        // number of sectors (physical blocks) stored on the disk media
    LogicalBlockCount   formatSectorCount;  // > 0 then formatting is supported and a format call takes 'formatSectorCount' sectors as input
} DiskInfo;

// Returns information about a disk drive.
// get_info(DiskInfo* _Nonnull pOutInfo)
#define kDiskCommand_GetInfo  IOResourceCommand(kDriverCommand_SubclassBase + 0)


typedef struct FormatSectorsRequest {
    MediaId                 mediaId;
    LogicalBlockAddress     addr;
    const void* _Nonnull    data;
    int                     status;
} FormatSectorsRequest;

// Formats 'formatSectorCount' consecutive sectors starting at sector 'addr'.
// 'data' must point to a memory block of size formatSectorCount * sectorSize
// bytes. 'addr' must be a multiple of formatSectorCount'. The caller will be
// blocked until all data has been written to disk or an error is encountered.
// format(const FormatSectorsRequest* _Nonnull req)
#define kDiskCommand_Format IOResourceCommand(kDriverCommand_SubclassBase + 1)


#if !defined(__KERNEL__)

// Synchronously writes all dirty disk blocks back to disk.
extern void Sync(void);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_DISK_H */

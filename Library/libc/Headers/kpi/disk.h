//
//  kpi/disk.h
//  libc
//
//  Created by Dietmar Planitzer on 10/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_DISK_H
#define _KPI_DISK_H 1

#include <kpi/ioctl.h>
#include <kpi/types.h>
#include <stdint.h>


// Disk/media properties
enum {
    kDisk_IsRemovable = 0x0001,
    kDisk_IsReadOnly = 0x0002,
};


// General information about a disk drive and the currently loaded media
typedef struct diskinfo {
    scnt_t      sectorCount;        // number of sectors (physical blocks) stored on the disk media
    scnt_t      rwClusterSize;      // > 1 then the number of consecutive sectors that should be read/written in one go for optimal disk I/O performance (eg drive wants you to read a whole track rather than individual sectors)
    size_t      sectorSize;         // size of a sector (physical block) stored on the disk media. Only relevant if you want to display this value to the user or format a disk
    uint32_t    properties;         // Disk/media properties 
    uint32_t    diskId;             // unique id starting at 1, incremented every time a new disk is inserted into the drive
} diskinfo_t;

// Returns information about a disk drive.
// get_info(diskinfo_t* _Nonnull pOutInfo)
#define kDiskCommand_GetInfo  IOResourceCommand(kDriverCommand_SubclassBase + 0)


// Disk geometry information
typedef struct diskgeom {
    size_t  headsPerCylinder;
    size_t  sectorsPerTrack;
    size_t  cylindersPerDisk;
    size_t  sectorSize;
} diskgeom_t;

// Returns geometry information for the disk that is currently in the drive.
// ENOMEDIUM is returned if no disk is in the drive.
// get_geometry(diskgeom_t* _Nonnull pOutGeometry)
#define kDiskCommand_GetGeometry  IOResourceCommand(kDriverCommand_SubclassBase + 1)

    
// Formats a track of 'sectorsPerTrack' consecutive sectors starting at the
// current position (rounded down to the closest track start). 'data' points to
// sectorSize * sectorsPerTrack bytes that should be written to the sectors in
// the track. The data portion of all sectors in the track are filled with zeros
// if 'data' is NULL. 'options' are options that control how the format command
// should execute. The caller will be blocked until all data has been written to
// disk or an error is encountered.
// format(const void* _Nullable data, unsigned int options)
#define kDiskCommand_FormatTrack    IOResourceCommand(kDriverCommand_SubclassBase + 2)


// Checks whether a disk was inserted into the drive and updates the drive state
// accordingly. You should call this function after receiving a EDISKCHANGE error
// from any of the other disk related calls. Returns EOK if a disk is in the drive
// and ENOMEDIUM if no disk is in the drive.
// sensedisk(void)
#define kDiskCommand_SenseDisk  IOResourceCommand(kDriverCommand_SubclassBase + 3)

#endif /* _KPI_DISK_H */

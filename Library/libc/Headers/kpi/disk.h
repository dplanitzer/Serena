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


// Drive family
enum {
    kDriveFamily_Floppy = 0,
    kDriveFamily_Fixed,           // aka hard disk
    kDriveFamily_CD,
    kDriveFamily_SSD,
    kDriveFamily_USBStick,
    kDriveFamily_RAM,
    kDriveFamily_ROM
};

// Platter diameter
enum {
    kPlatter_None = 0,
    kPlatter_2_5 = 63,
    kPlatter_3 = 76,
    kPlatter_3_5 = 89,
    kPlatter_5_25 = 133,
    kPlatter_8 = 203
};

// Drive properties
enum {
    kDrive_Fixed = 0x0001,
    kDrive_IsReadOnly = 0x0002,
};

// Information about the disk drive
typedef struct drive_info {
    uint16_t    family;
    uint16_t    platter;
    uint32_t    properties;         // drive properties 
} drive_info_t;

// Returns information about a disk drive.
// get_drive_info(drive_info_t* _Nonnull pOutInfo)
#define kDiskCommand_GetDriveInfo   IOResourceCommand(kDriverCommand_SubclassBase + 0)


// Disk properties
enum {
    kDisk_IsRemovable = 0x0001,
    kDisk_IsReadOnly = 0x0002,
};

// Disk information
typedef struct disk_info {
    size_t      heads;              // heads per cylinder
    size_t      cylinders;
    scnt_t      sectorsPerTrack;
    scnt_t      sectorsPerDisk;
    scnt_t      sectorsPerRdwr;     // number of consecutive sectors that the drive hardware reads/writes from/to this disk. Usually 1 but may be the same as 'sectorsPerTrack' if the disk hardware reads/writes whole tracks in a single I/O operation. May be used to implement sector clustering
    size_t      sectorSize;
    uint32_t    diskId;             // unique id starting at 1, incremented every time a new disk is inserted into the drive
    uint32_t    properties;         // Disk properties
} disk_info_t;

// Returns information about the disk that is currently in the drive.
// ENOMEDIUM is returned if no disk is in the drive.
// get_disk_info(disk_info_t* _Nonnull pOutInfo)
#define kDiskCommand_GetDiskInfo    IOResourceCommand(kDriverCommand_SubclassBase + 1)


// Formats a track of 'sectorsPerTrack' consecutive sectors starting at the
// current position (rounded down to the closest track start). The data portion
// of every sector is filled with 'fillByte'. The caller will be blocked until
// all data has been written to disk or an error is encountered.
// format(char fillByte)
#define kDiskCommand_FormatTrack    IOResourceCommand(kDriverCommand_SubclassBase + 2)


// Checks whether a disk was inserted into the drive and updates the drive state
// accordingly. You should call this function after receiving a EDISKCHANGE error
// from any of the other disk related calls. Returns EOK if a disk is in the drive
// and ENOMEDIUM if no disk is in the drive.
// sensedisk(void)
#define kDiskCommand_SenseDisk  IOResourceCommand(kDriverCommand_SubclassBase + 3)

#endif /* _KPI_DISK_H */

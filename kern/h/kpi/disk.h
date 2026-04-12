//
//  kpi/disk.h
//  kpi
//
//  Created by Dietmar Planitzer on 10/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_DISK_H
#define _KPI_DISK_H 1

#include <kpi/ioctl.h>
#include <kpi/types.h>
#include <stdint.h>


// Disk platter diameters
#define DISK_DIAM_UNKNOWN   0
#define DISK_DIAM_2_5       250
#define DISK_DIAM_3         300
#define DISK_DIAM_3_5       350
#define DISK_DIAM_5_25      525
#define DISK_DIAM_8         800


// Drive flags
#define DRIVE_FLAG_FIXED        0x0001
#define DRIVE_FLAG_READ_ONLY    0x0002

// Information about the disk drive
typedef struct drive_info {
    uint16_t    platter;
    uint32_t    flags;          // drive flags 
} drive_info_t;

// Returns information about a disk drive. Use the kDriverCommand_GetCategories
// call and look at the IODISK category to find out what kind of disk drive this
// is.
// get_drive_info(drive_info_t* _Nonnull pOutInfo)
#define kDiskCommand_GetDriveInfo   IOResourceCommand(kDriverCommand_SubclassBase + 0)


// Disk flags
#define DISK_FLAG_REMOVABLE 0x0001
#define DISK_FLAG_READ_ONLY 0x0002

// Disk information
typedef struct disk_info {
    size_t      heads;              // heads per cylinder
    size_t      cylinders;
    scnt_t      sectorsPerTrack;
    scnt_t      sectorsPerDisk;
    scnt_t      sectorsPerRdwr;     // number of consecutive sectors that the drive hardware reads/writes from/to this disk. Usually 1 but may be the same as 'sectorsPerTrack' if the disk hardware reads/writes whole tracks in a single I/O operation. May be used to implement sector clustering
    size_t      sectorSize;
    uint32_t    diskId;             // unique id starting at 1, incremented every time a new disk is inserted into the drive
    uint32_t    flags;              // disk flags
} disk_info_t;

// Returns information about the disk that is currently in the drive.
// ENOMEDIUM is returned if no disk is in the drive.
// get_disk_info(disk_info_t* _Nonnull pOutInfo)
#define kDiskCommand_GetDiskInfo    IOResourceCommand(kDriverCommand_SubclassBase + 1)


// Formats all sectors on the disk. 'The data portion of all sectors is filled
// with the byte 'fillByte'. Blocks the caller until the whole disk has been
// formatted. Returns ENOTSUP if this command is not supported by the disk
// driver. You should attempt to use FormatTrack() instead in this case.
// format_disk(char fillByte)
#define kDiskCommand_FormatDisk     IOResourceCommand(kDriverCommand_SubclassBase + 2)


// Formats a track of 'sectorsPerTrack' consecutive sectors starting at the
// current position (rounded down to the closest track start). The data portion
// of every sector is filled with 'fillByte'. The caller will be blocked until
// all data has been written to disk or an error is encountered.
// format(char fillByte)
#define kDiskCommand_FormatTrack    IOResourceCommand(kDriverCommand_SubclassBase + 3)


// Checks whether a disk was inserted into the drive and updates the drive state
// accordingly. You should call this function after receiving a EDISKCHANGE error
// from any of the other disk related calls. Returns EOK if a disk is in the drive
// and ENOMEDIUM if no disk is in the drive.
// sensedisk(void)
#define kDiskCommand_SenseDisk  IOResourceCommand(kDriverCommand_SubclassBase + 4)

#endif /* _KPI_DISK_H */

//
//  DiskImageFormat.h
//  diskimage
//
//  Created by Dietmar Planitzer on 6/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DiskImageFormat_h
#define DiskImageFormat_h

#include <stdbool.h>
#include <stdint.h>

enum DiskImageFormatType {
    kDiskImage_Amiga_DD_Floppy,
    kDiskImage_Amiga_HD_Floppy,
    kDiskImage_Serena
};

typedef struct DiskImageFormat {
    const char*                 name;
    enum DiskImageFormatType    format;
    size_t                      blockSize;
    size_t                      blocksPerDisk;
} DiskImageFormat;


typedef struct DiskImage {
    enum DiskImageFormatType    format;
    
    // Disk geometry
    size_t                      cylindersPerDisk;
    size_t                      headsPerCylinder;
    size_t                      sectorsPerTrack;
    size_t                      bytesPerSector;

    // Physical media as stored in the disk image file
    size_t                      physicalOffset;     // Offset from disk image file start to where the actual medium is stored
    size_t                      physicalSize;       // Size in bytes, of the medium inside the disk image file
} DiskImage;

#endif /* DiskImageFormat_h */

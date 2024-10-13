//
//  di_DiskImageFormat.h
//  diskimage
//
//  Created by Dietmar Planitzer on 6/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef di_DiskImageFormat_h
#define di_DiskImageFormat_h

#include <stdbool.h>
#include <stdint.h>
#include <System/Types.h>

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

#endif /* di_DiskImageFormat_h */

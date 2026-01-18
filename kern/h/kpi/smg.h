//
//  smg.h
//  kpi
//
//  Created by Dietmar Planitzer on 6/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_SMG_H
#define _KPI_SMG_H 1

#include <stdint.h>

#define SMG_EXTENSION   "smg"
#define SMG_SIGNATURE   0x534d4720ul
#define SMG_HEADER_SIZE sizeof(SMG_Header)


// Disk image contents is write protected and should not be changed
#define SMG_OPTION_READONLY             0x01
// The 'blocksPerDisk' value stored in the header is 0 and the disk size is
// defined by the current size of the container that stores the disk image. Ie
// if the disk image resides in a file and this option is set, then the disk
// image 'blocksPerDisk' is floor((file_size - sizeof(SMG_Header)) / blockSize).
#define SMG_OPTION_BPD_TRACKS_CONTAINER 0x02


// Serena disk image (smg)
// Data is stored big endian (network byte order)
// The disk image data follows the header
typedef struct SMG_Header {
    uint32_t    signature;
    uint32_t    headerSize;         // Size including the signature
    uint64_t    physicalBlockCount; // Physical size of the disk image in terms of blocks
    uint64_t    logicalBlockCount;  // Logical/actual size of the disk represented by the disk image
    uint32_t    blockSize;
    uint32_t    options;
} SMG_Header;

#endif /* _KPI_SMG_H */

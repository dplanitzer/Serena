//
//  AmigaDiskFormat.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef AmigaDiskFormat_h
#define AmigaDiskFormat_h

#include <klib/klib.h>

// See http://lclevy.free.fr/adflib/adf_info.html
#define ADF_SECTOR_SIZE         512

#define ADF_DD_SECS_PER_TRACK   11
#define ADF_DD_HEADS_PER_CYL    2
#define ADF_DD_CYLS_PER_DISK    80

#define ADF_HD_SECS_PER_TRACK   22
#define ADF_HD_HEADS_PER_CYL    2
#define ADF_HD_CYLS_PER_DISK    80


#define MFM_PRESYNC_WORD    0xAAAA
#define MFM_SYNC_WORD       0x4489
#define ADF_FORMAT_V1       0xff


//
// MFM encoded sector
//

typedef uint16_t ADF_MFMPreSync[2]; // 2 * MFM_PRESYNC_WORD
typedef uint16_t ADF_MFMSync[2];    // 2 * MFM_SYNC_WORD


typedef struct ADF_MFMSectorInfo
{
    uint32_t    odd_bits;
    uint32_t    even_bits;
} ADF_MFMSectorInfo;


typedef struct ADF_MFMSectorLabel
{
    uint32_t    odd_bits[4];
    uint32_t    even_bits[4];
} ADF_MFMSectorLabel;


typedef struct ADF_MFMChecksum {
    uint32_t    odd_bits;       // MFM odd bits of checksum
    uint32_t    even_bits;      // MFM even bits of checksum
} ADF_MFMChecksum;


typedef struct ADF_MFMData {
    uint32_t    odd_bits[512 / sizeof(uint32_t)];   // MFM odd bits of sector data
    uint32_t    even_bits[512 / sizeof(uint32_t)];  // MFM even bits of sector data
} ADF_MFMData;


// A MFM encoded sector suitable for writing to disk
typedef struct ADF_MFMWriteSector {
    ADF_MFMPreSync          presync;
    ADF_MFMSync             sync;
    ADF_MFMSectorInfo       info;
    ADF_MFMSectorLabel      label;
    ADF_MFMChecksum         header_checksum;
    ADF_MFMChecksum         data_checksum;
    ADF_MFMData             data;
} ADF_MFMWriteSector;


// A MFM encoded sector as read off the disk assuming that the track is read with
// hardware syncing enabled
typedef struct ADF_MFMReadSector {
    uint16_t                sync;
    ADF_MFMSectorInfo       info;
    ADF_MFMSectorLabel      label;
    ADF_MFMChecksum         header_checksum;
    ADF_MFMChecksum         data_checksum;
    ADF_MFMData             data;
} ADF_MFMReadSector;


//
// Decoded sector
//

typedef struct ADF_SectorInfo
{
    uint8_t   format;     // Amiga 1.0 format 0xff
    uint8_t   track;
    uint8_t   sector;
    uint8_t   sectors_until_gap;
} ADF_SectorInfo;

typedef uint32_t    ADF_SectorLabel[4];

#endif /* AmigaDiskFormat_h */

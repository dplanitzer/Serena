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


#define MFM_SYNC_WORD   0x4489
#define ADF_FORMAT_V1   0xff


typedef struct ADF_Synchronization {
    uint16_t    sync[2];    // 2 * MFM_SYNC_WORD
} ADF_Synchronization;


typedef struct ADF_SectorHeader
{
    uint8_t   format;     // Amiga 1.0 format 0xff
    uint8_t   track;
    uint8_t   sector;
    uint8_t   seceow;
    uint32_t  zero;
    uint32_t  header_crc;
    uint32_t  data_crc;
} ADF_SectorHeader;


typedef struct ADF_Checksum {
    uint32_t    odd_bits;       // MFM odd bits of checksum
    uint32_t    even_bits;      // MFM even bits of checksum
} ADF_Checksum;


typedef struct ADF_SectorData {
    uint32_t    odd_bits[512 / sizeof(uint32_t)];   // MFM odd bits of sector data
    uint32_t    even_bits[512 / sizeof(uint32_t)];  // MFM even bits of sector data
} ADF_SectorData;


typedef struct ADF_EncodedSector {
    ADF_Synchronization     sync;
    ADF_SectorHeader        header;
    ADF_Checksum            header_checksum;
    ADF_Checksum            data_checksum;
    ADF_SectorData          data;
} ADF_EncodedSector;

#endif /* AmigaDiskFormat_h */

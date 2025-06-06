//
//  adf.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef adf_h
#define adf_h

#include <kern/types.h>

// See http://lclevy.free.fr/adflib/adf_info.html
#define ADF_MFM_SYNC_SIZE           8
#define ADF_MFM_SECTOR_SIZE         1080
#define ADF_SECTOR_DATA_SIZE        512
#define ADF_GAP_SIZE                1660
#define ADF_HEADS_PER_CYL           2
#define ADF_CYLS_PER_DISK           80

#define ADF_DD_SECS_PER_TRACK   11
#define ADF_HD_SECS_PER_TRACK   22

#define ADF_MAX_SECS_PER_TRACK  ADF_HD_SECS_PER_TRACK


#define ADF_MFM_PRESYNC     0xAAAA
#define ADF_MFM_SYNC        0x4489
#define ADF_FORMAT_V1       0xff


//
// MFM encoded sector
//

typedef uint16_t ADF_MFMSync[4];    // 2 * ADF_MFM_PRESYNC, 2 * ADF_MFM_SYNC


typedef struct ADF_MFMSectorInfo {
    uint32_t    odd_bits;
    uint32_t    even_bits;
} ADF_MFMSectorInfo;


typedef struct ADF_MFMSectorLabel {
    uint32_t    odd_bits[4];
    uint32_t    even_bits[4];
} ADF_MFMSectorLabel;


typedef struct ADF_MFMChecksum {
    uint32_t    odd_bits;       // MFM odd bits of checksum
    uint32_t    even_bits;      // MFM even bits of checksum
} ADF_MFMChecksum;


typedef struct ADF_MFMData {
    uint32_t    odd_bits[ADF_SECTOR_DATA_SIZE / sizeof(uint32_t)];   // MFM odd bits of sector data
    uint32_t    even_bits[ADF_SECTOR_DATA_SIZE / sizeof(uint32_t)];  // MFM even bits of sector data
} ADF_MFMData;


// The payload of a MFM sector. This is the data (header + user data) that follows
// the MFM sync words
typedef struct ADF_MFMSector {
    ADF_MFMSectorInfo       info;
    ADF_MFMSectorLabel      label;
    ADF_MFMChecksum         header_checksum;
    ADF_MFMChecksum         data_checksum;
    ADF_MFMData             data;
} ADF_MFMSector;


// A MFM sector and its sync mark
typedef struct ADF_MFMPhysicalSector {
    ADF_MFMSync     sync;
    ADF_MFMSector   payload;
} ADF_MFMPhysicalSector;


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
typedef uint32_t    ADF_Checksum;

typedef struct ADF_Sector {
    ADF_SectorInfo  info;
    ADF_SectorLabel label;
    uint8_t         data[ADF_SECTOR_DATA_SIZE];
} ADF_Sector;

#endif /* adf_h */

//
//  machine/amiga/zorro.h
//  libc
//
//  Created by Dietmar Planitzer on 1/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _MAC_AMIGA_ZORRO_H
#define _MAC_AMIGA_ZORRO_H 1

#include <_cmndef.h>
#include <stdint.h>
#include <stddef.h>
#include <kpi/ioctl.h>

__CPP_BEGIN

// Expansion board types
#define ZORRO_TYPE_RAM  0
#define ZORRO_TYPE_IO   1


// Expansion bus types
#define ZORRO_BUS_2 2
#define ZORRO_BUS_3 3


// This board does not accept a shut up command
#define ZORRO_FLAG_CANT_SHUTUP      0x01

// This expansion entry is related to the next one. Eg both are part of the same
// physical board (slot)
#define ZORRO_FLAG_NEXT_IS_RELATED  0x02


typedef struct zorro_conf {
    uint8_t* _Nonnull   start;          // base address
    size_t              physicalSize;   // size of memory space reserved for this board
    size_t              logicalSize;    // size of memory space actually occupied by the board
    uint32_t            serialNumber;
    uint16_t            manufacturer;
    uint16_t            product;
    int8_t              type;
    int8_t              bus;
    int8_t              slot;
    uint8_t             flags;
} zorro_conf_t;


// Returns the number of slots that contain cards.
// get_card_count(size_t* _Nonnull ncards)
#define kZorroCommand_GetCardCount  IOResourceCommand(kDriverCommand_SubclassBase + 0)

// Returns the configuration information for the card at index 'idx'.
// get_card_config(size_t idx, zorro_conf_t* _Nonnull cfg)
#define kZorroCommand_GetCardConfig IOResourceCommand(kDriverCommand_SubclassBase + 1)

__CPP_END

#endif /* _MAC_AMIGA_ZORRO_H */

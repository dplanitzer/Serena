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

__CPP_BEGIN

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

__CPP_END

#endif /* _MAC_AMIGA_ZORRO_H */

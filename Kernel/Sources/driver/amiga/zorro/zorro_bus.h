//
//  zorro_bus.h
//  Kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef zorro_bus_h
#define zorro_bus_h

#include <System/Types.h>


// Supported max number of expansion boards
#define ZORRO_BUS_CAPACITY  16

// Expansion board types
#define BOARD_TYPE_RAM  0
#define BOARD_TYPE_IO   1

// Expansion bus types
#define ZORRO_2_BUS 0
#define ZORRO_3_BUS 1


// An expansion board
typedef struct zorro_board {
    uint8_t* _Nonnull   start;          // base address
    size_t              physical_size;  // size of memory space reserved for this board
    size_t              logical_size;   // size of memory space actually occupied by the board
    uint32_t            serial_number;
    uint16_t            manufacturer;
    uint16_t            product;
    int8_t              type;
    int8_t              bus;
    int8_t              slot;
    int8_t              reserved;
} zorro_board_t;


typedef struct zorro_bus {
    size_t          count;
    zorro_board_t   board[ZORRO_BUS_CAPACITY];
} zorro_bus_t;


extern void zorro_auto_config(zorro_bus_t* _Nonnull bus);


//
// Internal
//

// Space for Zorro II auto configuration
#define ZORRO_2_CONFIG_BASE         ((uint8_t*)0x00e80000)

// Space for Zorro III auto configuration
#define ZORRO_3_CONFIG_BASE         ((uint8_t*)0xff000000)


// Space for Zorro II memory expansion boards
#define ZORRO_2_MEMORY_LOW          ((uint8_t*)0x00200000)
#define ZORRO_2_MEMORY_HIGH         ((uint8_t*)0x00a00000)

// Space for Zorro II I/O expansion boards
#define ZORRO_2_IO_LOW              ((uint8_t*)0x00e90000)
#define ZORRO_2_IO_HIGH             ((uint8_t*)0x00f00000)

// Extra Space for Zorro II I/O expansion boards available in Zorro 3 machines
#define ZORRO_2_EXTRA_IO_LOW        ((uint8_t*)0x00a00000)
#define ZORRO_2_EXTRA_IO_HIGH       ((uint8_t*)0x00b80000)

// Space for Zorro III (memory and I/O) expansion boards
#define ZORRO_3_EXPANSION_LOW       ((uint8_t*)0x10000000)
#define ZORRO_3_EXPANSION_HIGH      ((uint8_t*)0x80000000)


// This board does not accept a shut up command
#define ZORRO_FLAG_CANT_SHUTUP      0x01

// This expansion entry is related to the next one. Eg both are part of the same
// physical board (slot)
#define ZORRO_FLAG_NEXT_IS_RELATED  0x02


// Zorro board configuration information
typedef struct board_config {
    size_t      physical_size;  // physical board size
    size_t      logical_size;   // logical board size which may be smaller than the physical size; 0 means the kernel should auto-size the board
    uint32_t    serial_number;
    uint16_t    manufacturer;
    uint16_t    product;
    uint8_t     bus;
    uint8_t     type;
    uint8_t     flags;
    uint8_t     reserved;
} board_config_t;

#endif /* zorro_bus_h */

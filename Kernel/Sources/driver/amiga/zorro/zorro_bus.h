//
//  zorro_bus.h
//  Kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef zorro_bus_h
#define zorro_bus_h

#include <klib/Types.h>


// Supported max number of expansion boards
#define EXPANSION_BOARDS_CAPACITY    16

// Expanion board types
#define EXPANSION_TYPE_RAM  0
#define EXPANSION_TYPE_IO   1

// Expansion bus types
#define EXPANSION_BUS_ZORRO_2   0
#define EXPANSION_BUS_ZORRO_3   1


// An expansion board
typedef struct ExpansionBoard {
    uint8_t* _Nonnull   start;          // base address
    size_t              physical_size;  // size of memory space reserved for this board
    size_t              logical_size;   // size of memory space actually occupied by the board
    int8_t              type;
    int8_t              bus;
    int8_t              slot;
    int8_t              reserved;
    uint16_t            manufacturer;
    uint16_t            product;
    uint32_t            serial_number;
    // Update lowmem.i if you add a new property here
} ExpansionBoard;


typedef struct ExpansionBus {
    int                 board_count;
    ExpansionBoard      board[EXPANSION_BOARDS_CAPACITY];
} ExpansionBus;


extern void zorro_auto_config(ExpansionBus* pExpansionBus);


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
typedef struct Zorro_BoardConfiguration {
    size_t      physical_size;  // physical board size
    size_t      logical_size;   // logical board size which may be smaller than the physical size; 0 means the kernel should auto-size the board
    uint8_t     bus;
    uint8_t     type;
    uint8_t     flags;
    uint8_t     reserved;
    uint16_t    manufacturer;
    uint16_t    product;
    uint32_t    serial_number;
} Zorro_BoardConfiguration;

#endif /* zorro_bus_h */

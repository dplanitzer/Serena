//
//  zorro_bus.h
//  Kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef zorro_bus_h
#define zorro_bus_h

#include <ext/queue.h>
#include <machine/amiga/zorro.h>


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


// An expansion board
typedef struct zorro_board {
    ListNode        node;
    zorro_conf_t    cfg;
} zorro_board_t;


typedef struct zorro_bus {
    List/*<zorro_board_t>*/ boards;
    size_t                  count;
} zorro_bus_t;


extern void zorro_auto_config(zorro_bus_t* _Nonnull bus);
extern void zorro_destroy_bus(zorro_bus_t* _Nullable bus);

#endif /* zorro_bus_h */

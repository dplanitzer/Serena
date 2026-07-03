//
//  ZorroBus.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ZorroBus.h"
#include "ZorroDevice.h"
#include <hal/cpu.h>
#include <hal/sys_desc.h>
#include <hal/hw/m68k-amiga/chipset.h>
#include <kern/kalloc.h>
#include <kern/kernlib.h>
#include <machine/amiga/zorro.h>


// Maximum number of Zorro slots that we support. Amiga 2000 offered the most
// with 5. Some 3rd party bus boards extended this to 8
#define ZORRO_MAX_SLOT_COUNT    8


typedef struct zorro_bus {
    size_t          count;
    zorro_conf_t    conf[ZORRO_MAX_SLOT_COUNT];
} zorro_bus_t;


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



final_class_ivars(ZorroBus, IODriver,
);

IOCATS_DEF(g_cats, IOBUS_ZORRO);


errno_t ZorroBus_Create(ZorroBusRef _Nullable * _Nonnull pOutSelf)
{
    return IODriver_Create(class(ZorroBus), g_cats, (IODriverRef*)pOutSelf);
}


// Reads a byte value from the given Zorro auto configuration address
static uint8_t z_read(volatile uint8_t* _Nonnull addr, bool invert, bool isZorro3)
{
    const size_t offset = (isZorro3) ? 0x100 : 0x002;
    volatile uint8_t* pHigh8 = addr;
    volatile uint8_t* pLow8 = addr + offset;
    uint8_t byte = (pHigh8[0] & 0xf0) | ((pLow8[0] >> 4) & 0x0f);
    
    return (invert) ? ~byte : byte;
}

// Probes the autoconfig area for the presence of an expansion board. Returns true
// if a board was found and false otherwise. 'cfg' contains information about
// the board that was found. The contents of 'cfg' is undefined if no board was
// found.
// NOTE: We do not check whether cards actually return 0 for auto config locations
// for which they are supposed to return 0 according to the spec because at least
// some cards do in fact return non-zero values. Eg Commodore A2091 SCSI card.
static bool z_read_config_space(zorro_conf_t* _Nonnull cfg, uint8_t busType)
{
    const bool isZorro3 = (busType == ZORRO_BUS_3);
    register uint8_t* pAutoConfigBase = (isZorro3) ? ZORRO_3_CONFIG_BASE : ZORRO_2_CONFIG_BASE;
    
    // See: http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node02C7.html
    // See: http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node02C8.html

    // Type
    const uint8_t type = z_read(pAutoConfigBase + 0x00, false, isZorro3);
    switch((type >> 6) & 0x03) {
        case 0:     return false;
        case 1:     return false;
        case 2:     cfg->bus = ZORRO_BUS_3; break;
        case 3:     cfg->bus = ZORRO_BUS_2; break;
        default:    abort();
    }
    
    if (type & (1 << 5)) {
        cfg->type = ZORRO_TYPE_RAM;
    } else {
        cfg->type = ZORRO_TYPE_IO;
    }
    if (type & (1 << 3)) {
        cfg->flags |= ZORRO_FLAG_NEXT_IS_RELATED;
    }
    
    
    // Product
    cfg->product = (uint16_t)z_read(pAutoConfigBase + 0x04, true, isZorro3);
    
    
    // Flags
    const uint8_t flags = z_read(pAutoConfigBase + 0x08, true, isZorro3);
    if (flags & (1 << 6)) {
        cfg->flags |= ZORRO_FLAG_CANT_SHUTUP;
    }
    
    const bool isExtendedSize = (cfg->bus == ZORRO_BUS_3) && (flags & (1 <<5)) != 0;
    const uint8_t physsiz = type & 0x07;
    static const size_t gBoardSizes[8] = {
        SIZE_MB(8),   SIZE_KB(64), SIZE_KB(128), SIZE_KB(256),
        SIZE_KB(512), SIZE_MB(1),  SIZE_MB(2),   SIZE_MB(4)
    };
    static const size_t gExtendedBoardSizes[8] = {
        SIZE_MB(16),  SIZE_MB(32),  SIZE_MB(64), SIZE_MB(128),
        SIZE_MB(256), SIZE_MB(512), SIZE_GB(1),  0
    };
    
    cfg->physicalSize = (isExtendedSize) ? gExtendedBoardSizes[physsiz] : gBoardSizes[physsiz];
    if (cfg->physicalSize == 0) {
        return false;
    }

    static const size_t gLogicalSize[12] = {
        SIZE_KB(64), SIZE_KB(128), SIZE_KB(256), SIZE_KB(512),
        SIZE_MB(1),  SIZE_MB(2),   SIZE_MB(4),   SIZE_MB(6),
        SIZE_MB(8),  SIZE_MB(10),  SIZE_MB(12),  SIZE_MB(14)
    };
    
    const uint8_t logsiz = (cfg->bus == ZORRO_BUS_3) ? flags & 0x0f : 0;
    switch (logsiz) {
        case 0x00: cfg->logicalSize = cfg->physicalSize; // logical size is same as physical size
            break;
        case 0x01: cfg->logicalSize = 0; // automatically sized by the kernel
            break;
        default:
            if (logsiz > 13) {
                return false;
            } else {
                cfg->logicalSize = gLogicalSize[logsiz - 2];
            }
            break;
    }

    
    // Manufacturer
    const uint8_t manuHigh = z_read(pAutoConfigBase + 0x10, true, isZorro3);
    const uint8_t manuLow = z_read(pAutoConfigBase + 0x14, true, isZorro3);
    
    cfg->manufacturer = (manuHigh << 8) | manuLow;
    if (cfg->manufacturer == 0) {
        return false;
    }
    
    
    // Serial number
    const uint8_t sernum3 = z_read(pAutoConfigBase + 0x18, true, isZorro3);
    const uint8_t sernum2 = z_read(pAutoConfigBase + 0x1c, true, isZorro3);
    const uint8_t sernum1 = z_read(pAutoConfigBase + 0x20, true, isZorro3);
    const uint8_t sernum0 = z_read(pAutoConfigBase + 0x24, true, isZorro3);
    
    cfg->serialNumber = (sernum3 << 24) | (sernum2 << 16) | (sernum1 << 8) | sernum0;
    
    
    // 0x28 & 0x2c -> optional ROM vector
    
    return true;
}

static void z2_shutup(void)
{
    volatile uint8_t* pNybble1 = ((uint8_t*)(ZORRO_2_CONFIG_BASE + 0x4c));
    volatile uint8_t* pNybble0 = ((uint8_t*)(ZORRO_2_CONFIG_BASE + 0x4e));
    
    pNybble0[0] = 0;
    pNybble1[0] = 0;
}

static void z3_shutup(void)
{
    volatile uint8_t* pAddr = ((uint8_t*)(ZORRO_3_CONFIG_BASE + 0x4c));
    
    pAddr[0] = 0;
}

// Tells the board which is currently visible in the auto config space to
// shut up. This causes the bus to make the next board in the chain available
// for configuration. The shut up board will enter idle state until the next
// system reset.
static void z_shutup(uint8_t bus)
{
    if (bus == ZORRO_BUS_3) {
        z3_shutup();
    } else {
        z2_shutup();
    }
}

static void z2_assign_base_address(uint8_t* _Nullable addr)
{
    uint16_t top16 = ((unsigned long) addr) >> 16;
    
    volatile uint8_t* pNybble3 = ((uint8_t*)(ZORRO_2_CONFIG_BASE + 0x44));
    volatile uint8_t* pNybble2 = ((uint8_t*)(ZORRO_2_CONFIG_BASE + 0x46));
    volatile uint8_t* pNybble1  = ((uint8_t*)(ZORRO_2_CONFIG_BASE + 0x48));
    volatile uint8_t* pNybble0  = ((uint8_t*)(ZORRO_2_CONFIG_BASE + 0x4a));
    const uint8_t nybble3 = ((top16 >> 12) & 0x000f);
    const uint8_t nybble2 = ((top16 >>  8) & 0x000f);
    const uint8_t nybble1 = ((top16 >>  4) & 0x000f);
    const uint8_t nybble0 = ((top16 >>  0) & 0x000f);
    const uint8_t zNybble3 = (nybble3 << 4) | nybble3;
    const uint8_t zNybble2 = (nybble2 << 4) | nybble2;
    const uint8_t zNybble1 = (nybble1 << 4) | nybble1;
    const uint8_t zNybble0 = (nybble0 << 4) | nybble0;

    pNybble2[0] = zNybble2;
    pNybble3[0] = zNybble3;
    pNybble0[0] = zNybble0;
    pNybble1[0] = zNybble1;
}

static void z3_assign_base_address(uint8_t* _Nullable addr)
{
    uint16_t top16 = ((unsigned int) addr) >> 16;
    
    volatile uint8_t* pByte1 = ((uint8_t*)(ZORRO_3_CONFIG_BASE + 0x44));
    volatile uint8_t* pByte0  = ((uint8_t*)(ZORRO_3_CONFIG_BASE + 0x48));
    uint8_t byte1 = (top16 >> 8) & 0x00ff;
    uint8_t byte0  = (top16 >> 0) & 0x00ff;
    
    pByte0[0] = byte0;
    pByte1[0] = byte1;
}

// Assigns the given address as the base address to the board currently visible
// in the auto config space. This moves the board to the new address and the next
// board becomes visible in auto config space.
static void z_assign_base_address(uint8_t* _Nullable addr, uint8_t bus)
{
    if (bus == ZORRO_BUS_3) {
        z3_assign_base_address(addr);
    } else {
        z2_assign_base_address(addr);
    }
}

static uint8_t* _Nonnull z2_align_board_address(uint8_t* _Nonnull base_ptr, unsigned int board_size, bool isMemory)
{
    if (isMemory && board_size == SIZE_MB(8)) {
        // Can fit one board
        return (base_ptr == ZORRO_2_MEMORY_LOW) ? ZORRO_2_MEMORY_LOW : ZORRO_2_MEMORY_HIGH;
    } else if (isMemory && board_size == SIZE_MB(4)) {
        // Can fit up to two boards
        if (base_ptr == ZORRO_2_MEMORY_LOW) {
            return ZORRO_2_MEMORY_LOW;
        } else if (base_ptr <= ZORRO_2_MEMORY_LOW + SIZE_MB(2)) {
            return ZORRO_2_MEMORY_LOW + SIZE_MB(2);
        } else if (base_ptr <= ZORRO_2_MEMORY_LOW + SIZE_MB(4)) {
            return ZORRO_2_MEMORY_LOW + SIZE_MB(4);
        } else {
            return ZORRO_2_MEMORY_HIGH;
        }
    } else {
        return __Ceil_Ptr_PowerOf2(base_ptr, board_size);
    }
}

static uint8_t* _Nullable z_calc_base_address_for_board_in_range(const zorro_conf_t* _Nonnull cfg, const zorro_bus_t* _Nonnull bus, uint8_t* board_space_base_addr, uint8_t* board_space_top_addr)
{
    const bool isMemoryBoard = cfg->type == ZORRO_TYPE_RAM;
    const bool isZorro3Board = cfg->bus == ZORRO_BUS_3;
    uint8_t* highest_occupied_board_addr = board_space_base_addr;
    const zorro_conf_t* pHighestAllocatedBoard = NULL;

    // Find the board with a matching Zorro bus, board type and expansion space
    // address range that has the highest assigned address
    for (size_t i = 0; i < bus->count; i++) {
        const zorro_conf_t* cp = &bus->conf[i];

        if (cp->bus == cfg->bus
            && cp->type == cfg->type
            && cp->start >= board_space_base_addr
            && cp->start < board_space_top_addr) {
            if (cp->start >= highest_occupied_board_addr) {
                highest_occupied_board_addr = cp->start;
                pHighestAllocatedBoard = cp;
            }
        }
    }


    // Calculate the address for the new board. It'll occupy the space just above the board we found.
    uint8_t* board_base_addr;
    if (pHighestAllocatedBoard != NULL) {
        uint8_t* proposed_board_base_addr = pHighestAllocatedBoard->start + pHighestAllocatedBoard->physicalSize;

        if (isZorro3Board) {
            board_base_addr = __Ceil_Ptr_PowerOf2(proposed_board_base_addr, cfg->physicalSize);
        } else {
            board_base_addr = z2_align_board_address(proposed_board_base_addr, cfg->physicalSize, isMemoryBoard);
        }
    } else {
        board_base_addr = board_space_base_addr;
    }
    uint8_t* board_top_addr = board_base_addr + cfg->physicalSize;
    
    return (board_top_addr <= board_space_top_addr) ? board_base_addr : NULL;
}

static uint8_t* _Nullable z_calc_base_address_for_board(const zorro_conf_t* _Nonnull cfg, const zorro_bus_t* _Nonnull bus)
{
    uint8_t* board_base_addr = NULL;

    if (cfg->bus == ZORRO_BUS_3) {
        board_base_addr = z_calc_base_address_for_board_in_range(cfg, bus, ZORRO_3_EXPANSION_LOW, ZORRO_3_EXPANSION_HIGH);
    } else {
        if (cfg->type == ZORRO_TYPE_RAM) {
            board_base_addr = z_calc_base_address_for_board_in_range(cfg, bus, ZORRO_2_MEMORY_LOW, ZORRO_2_MEMORY_HIGH);
        } else {
            board_base_addr = z_calc_base_address_for_board_in_range(cfg, bus, ZORRO_2_IO_LOW, ZORRO_2_IO_HIGH);
            if (board_base_addr == NULL && chipset_get_ramsey_version() > 0) {
                // Zorro 3 based machines support an extra Zorro 2 I/O address range
                board_base_addr = z_calc_base_address_for_board_in_range(cfg, bus, ZORRO_2_EXTRA_IO_LOW, ZORRO_2_EXTRA_IO_HIGH);
            }
        }
    }

    return board_base_addr;
}

// Dynamically determines the size of the given memory expansion board.
static size_t z3_auto_size_memory_board(zorro_conf_t* _Nonnull cfg)
{
    uint8_t* pLower = cfg->start;
    uint8_t* pUpper = cfg->start + cfg->physicalSize;
    size_t size = 0;
    
    while (pLower < pUpper && (pUpper - pLower) >= 4) {
        if (cpu_verify_ram_4b(pLower)) {
            break;
        }
        
        pLower += SIZE_KB(512);
        size += SIZE_KB(512);
    }
    
    return size;
}



static zorro_bus_t* _Nullable _auto_config_bus(void)
{
    zorro_bus_t* bus;
    const bool isZorro3 = chipset_get_ramsey_version() > 0;

    if (kalloc_cleared(sizeof(zorro_bus_t), (void**)&bus) != EOK) {
        return NULL;
    }


    for (;;) {
        zorro_conf_t* cfg = &bus->conf[bus->count];

        if (!z_read_config_space(cfg, ZORRO_BUS_2)) {
            if (!isZorro3 || (isZorro3 && !z_read_config_space(cfg, ZORRO_BUS_3))) {
                break;
            }
        }

        // calculate the base address for RAM or I/O. Growing bottom to top
        uint8_t* board_base_addr = z_calc_base_address_for_board(cfg, bus);

        
        // check whether we still got enough space left to map the board. If not then
        // shut the board up and move on to the next one.
        if (board_base_addr == NULL) {
            // Have to stop looking for more boards if we can't shut this one up
            // because this means that we can't make the next board visible in the
            // config area...
            if ((cfg->flags & ZORRO_FLAG_CANT_SHUTUP) == 0) {
                z_shutup(cfg->bus);
                continue;
            } else {
                break;
            }
        }
        
        
        // assign the start address to the board and update our board address vars
        z_assign_base_address(board_base_addr, cfg->bus);

        
        // update the board config and add the board to our bus list
        cfg->start = board_base_addr;
        cfg->slot = (int8_t)bus->count;

        bus->count++;


        // if RAM and logical_size == 0, auto-size the board
        if (cfg->logicalSize == 0) {
            if (cfg->type == ZORRO_TYPE_RAM) {
                cfg->logicalSize = z3_auto_size_memory_board(cfg);
            } else {
                // This is really a hardware bug. Auto sizing for I/O boards makes no
                // sense 'cause there's no safe way to read / write registers blindly.
                cfg->logicalSize = cfg->physicalSize;
            }
        }
    }

    return bus;
}

static void ZorroBus_ScanBus(ZorroBusRef _Nonnull self)
{
    // Auto config the Zorro bus
    zorro_bus_t* bus = _auto_config_bus();
    if (bus == NULL) {
        return;
    }


    // Create a ZorroDevice instance for each slot and start it
    for (size_t i = 0; i < bus->count; i++) {
        const zorro_conf_t* cfg = &bus->conf[i];
        ZorroDeviceRef dp;
        
        if (ZorroDevice_Create(cfg, &dp) == EOK) {
            IODriver_Launch(dp, (IODriverRef)self);
            Object_Release(dp);
        }
    }

    kfree(bus);
}

void ZorroBus_onLaunched(ZorroBusRef _Nonnull self)
{
    // Auto-config the bus. Discover as many cards as possible and ignore anything
    // that fails.
    ZorroBus_ScanBus(self);
}


bool ZorroBus_isExclusive(ZorroBusRef _Nonnull self)
{
    return false;
}


class_func_defs(ZorroBus, IODriver,
override_func_def(onLaunched, ZorroBus, IODriver)
override_func_def(isExclusive, ZorroBus, IODriver)
);

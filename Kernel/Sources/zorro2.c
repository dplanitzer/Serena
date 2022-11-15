//
//  zorro2.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/10/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Platform.h"
#include "Bytes.h"
#include "SystemDescription.h"


// Space for Zorro II auto configuration
#define ZORRO_2_CONFIG_LOW          ((Byte*)0x00e80000)
#define ZORRO_2_CONFIG_HIGH         ((Byte*)0x00f00000)

// Space for Zorro II memory expansion boards
#define ZORRO_2_MEMORY_LOW          ((Byte*)0x00200000)
#define ZORRO_2_MEMORY_HIGH         ((Byte*)0x009fffff)

// Space for Zorro II I/O expansion boards
#define ZORRO_2_IO_LOW              ((Byte*)0x00e90000)
#define ZORRO_2_IO_HIGH             ((Byte*)0x00efffff)

// Space for Zorro II I/O expansion boards (A3000+)
#define ZORRO_2_IO_EXT_LOW          ((Byte*)0x00a00000)
#define ZORRO_2_IO_EXT_HIGH         ((Byte*)0x00b80000)


// This board does not accept a shutup command
#define ZORRO_2_FLAG_CANT_SHUTUP        0x01

// This expansion entry is related to the next one. Eg both are part of teh same
// physical board (slot)
#define ZORRO_2_FLAG_NEXT_IS_RELATED    0x02

// This is a RA board (rather than an I/O board)
#define ZORRO_2_FLAG_IS_MEMORY          0x04


// Zorro II config info
typedef struct _Zorro2_Configuration {
    UInt    size;           // board size
    UInt16  flags;
    UInt16  manufacturer;
    UInt16  product;
    UInt32  serial_number;
} Zorro2_Configuration;



// Reads a byte value from the given Zorro II autoconfig address
static Byte zorro2_read(volatile Byte* addr, Bool invert)
{
    volatile Byte* pHigh8 = addr;
    volatile Byte* pLow8 = addr + 2;
    Byte byte = (pHigh8[0] & 0xf0) | ((pLow8[0] >> 4) & 0x0f);
    
    return (invert) ? ~byte : byte;
}

// Probes the autoconfig area for the presence of a expansion board. Returns true
// if a board was found and false otherwise. 'board' contains information about
// the board that was found. The contents of 'board' is undefined if no board was
// found.
static Bool zorro2_read_config_space(Zorro2_Configuration* config)
{
    register Byte* pAutoConfigBase = ZORRO_2_CONFIG_LOW;
    
    // See: http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node02C7.html
    // See: http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node02C8.html
    Bytes_ClearRange((Byte*)config, sizeof(Zorro2_Configuration));
    
    
    // Type
    const UInt8 type = (UInt8)zorro2_read(pAutoConfigBase + 0x00, false);
    if ((type >> 6) != 3) {
        return false;
    }
    
    if (type & (1 << 5)) {
        config->flags |= ZORRO_2_FLAG_IS_MEMORY;
    }
    if (type & (1 << 3)) {
        config->flags |= ZORRO_2_FLAG_NEXT_IS_RELATED;
    }
    
    
    // Product
    config->product = (UInt16)zorro2_read(pAutoConfigBase + 0x04, true);
    
    
    // Flags
    const UInt8 flags = (UInt8)zorro2_read(pAutoConfigBase + 0x08, true);
    
    if (flags & (1 << 6)) {
        config->flags |= ZORRO_2_FLAG_CANT_SHUTUP;
    }
    
    static const UInt gBoardSizes[8] = {
        SIZE_MB(8), SIZE_KB(64), SIZE_KB(128), SIZE_KB(256),
        SIZE_KB(512), SIZE_MB(1), SIZE_MB(2), SIZE_MB(4)
    };
    config->size = gBoardSizes[type & 0x07];
    
    
    // Reserved
    if (zorro2_read(pAutoConfigBase + 0x0c, true) != 0) {
        return false;
    }
    
    
    // Manufacturer
    const UInt8 manuHigh = (UInt8)zorro2_read(pAutoConfigBase + 0x10, true);
    const UInt8 manuLow = (UInt8)zorro2_read(pAutoConfigBase + 0x14, true);
    
    config->manufacturer = (manuHigh << 8) | manuLow;
    
    
    // Serial number
    const UInt8 sernum3 = (UInt8)zorro2_read(pAutoConfigBase + 0x18, true);
    const UInt8 sernum2 = (UInt8)zorro2_read(pAutoConfigBase + 0x1c, true);
    const UInt8 sernum1 = (UInt8)zorro2_read(pAutoConfigBase + 0x20, true);
    const UInt8 sernum0 = (UInt8)zorro2_read(pAutoConfigBase + 0x24, true);
    
    config->serial_number = (sernum3 << 24) | (sernum2 << 16) | (sernum1 << 8) | sernum0;
    
    
    // Reserved
    if (zorro2_read(pAutoConfigBase + 0x34, true) != 0) {
        return false;
    }
    if (zorro2_read(pAutoConfigBase + 0x38, true) != 0) {
        return false;
    }
    if (zorro2_read(pAutoConfigBase + 0x3c, true) != 0) {
        return false;
    }
    if (zorro2_read(pAutoConfigBase + 0x50, true) != 0) {
        return false;
    }
    if (zorro2_read(pAutoConfigBase + 0x54, true) != 0) {
        return false;
    }
    if (zorro2_read(pAutoConfigBase + 0x58, true) != 0) {
        return false;
    }
    if (zorro2_read(pAutoConfigBase + 0x5c, true) != 0) {
        return false;
    }
    if (zorro2_read(pAutoConfigBase + 0x60, true) != 0) {
        return false;
    }
    if (zorro2_read(pAutoConfigBase + 0x64, true) != 0) {
        return false;
    }
    if (zorro2_read(pAutoConfigBase + 0x68, true) != 0) {
        return false;
    }
    if (zorro2_read(pAutoConfigBase + 0x6c, true) != 0) {
        return false;
    }
    if (zorro2_read(pAutoConfigBase + 0x70, true) != 0) {
        return false;
    }
    if (zorro2_read(pAutoConfigBase + 0x74, true) != 0) {
        return false;
    }
    if (zorro2_read(pAutoConfigBase + 0x78, true) != 0) {
        return false;
    }
    if (zorro2_read(pAutoConfigBase + 0x7c, true) != 0) {
        return false;
    }
    
    return true;
}

// Tells the board which is currently visible in the auto config space to
// shut up. This causes the bus to make teh next board in the chain available
// for configuration. The shut up board will enter idle state until the next
// system reset.
static void zorro2_auto_config_shutup(void)
{
    volatile UInt16* pAddr = ((UInt16*)(ZORRO_2_CONFIG_LOW + 0x4c));
    
    *pAddr = 0xdead;
}

// Assigns the given address as the base address to the board currently visible
// in the auto config space. This moves the board to the new address and the next
// board becomes visible in auto config space.
static void zorro2_auto_config_assign_base_address(Byte* addr)
{
    UInt16 top16 = ((UInt) addr) >> 16;
    
    volatile Byte* pNybble3 = ((Byte*)(ZORRO_2_CONFIG_LOW + 0x44));
    volatile Byte* pNybble2 = ((Byte*)(ZORRO_2_CONFIG_LOW + 0x46));
    volatile Byte* pNybble1  = ((Byte*)(ZORRO_2_CONFIG_LOW + 0x48));
    volatile Byte* pNybble0  = ((Byte*)(ZORRO_2_CONFIG_LOW + 0x4a));
    const Byte nybble3 = ((top16 >> 12) & 0x000f) << 4;
    const Byte nybble2 = ((top16 >>  8) & 0x000f) << 4;
    const Byte nybble1 = ((top16 >>  4) & 0x000f) << 4;
    const Byte nybble0 = ((top16 >>  0) & 0x000f) << 4;
    
    volatile Byte* pHigh8 = ((Byte*)(ZORRO_2_CONFIG_LOW + 0x44));
    volatile Byte* pLow8  = ((Byte*)(ZORRO_2_CONFIG_LOW + 0x48));
    const Byte high8 = (top16 >> 8) & 0x00ff;
    const Byte low8  = (top16 >> 0) & 0x00ff;
    
    *pNybble2 = nybble2;
    *pNybble3 = nybble3;
    *pHigh8 = high8;
    
    *pNybble0 = nybble0;
    *pNybble1 = nybble1;
    *pLow8 = low8;
}


void zorro2_auto_config(SystemDescription* pSysDesc)
{
    Byte* zorro_2_memory_expansion_addr = ZORRO_2_MEMORY_LOW;
    Byte* zorro_2_io_expansion_addr = ZORRO_2_IO_LOW;
    UInt8 prev_config_flags = ZORRO_2_FLAG_NEXT_IS_RELATED;
    Int slot = 0;
    
    pSysDesc->expansion_board_count = 0;

    while (pSysDesc->expansion_board_count < EXPANSION_BOARDS_CAPACITY) {
        Zorro2_Configuration config;
        
        const Bool isValid = zorro2_read_config_space(&config);
        if (!isValid) {
            break;
        }
        
        // compute the base address for RAM or I/O
        const Bool isMemory = (config.flags & ZORRO_2_FLAG_IS_MEMORY) != 0;
        const UInt board_size = align_uint(config.size, SIZE_KB(64));
        Byte* board_low_addr;
        Byte* board_high_addr;
        
        // Zorro 2: separate mem and I/O spaces. Growing bottom to top
        if (isMemory) {
            board_low_addr = zorro_2_memory_expansion_addr;
            board_high_addr = board_low_addr + board_size;
        } else {
            board_low_addr = zorro_2_io_expansion_addr;
            board_high_addr = board_low_addr + board_size;
        }
        
        
        // check whether we still got enough space left to map the board. If not then
        // shut the board up and move on to the next one.
        Byte* avail_low_addr;
        Byte* avail_high_addr;
        
        if (isMemory) {
            avail_low_addr = zorro_2_memory_expansion_addr;
            avail_high_addr = ZORRO_2_MEMORY_HIGH;
        } else {
            avail_low_addr = zorro_2_io_expansion_addr;
            avail_high_addr = ZORRO_2_IO_HIGH;
        }
        
        const Bool fits = (board_low_addr >= avail_low_addr && board_high_addr <= avail_high_addr);
        if (!fits) {
            // Have to stop looking for more boards if we can't shut this one up
            // because this means that we can't make the next board visible in the
            // config area...
            if ((config.flags & ZORRO_2_FLAG_CANT_SHUTUP) == 0) {
                zorro2_auto_config_shutup();
                continue;
            } else {
                break;
            }
        }
        
        
        // assign the start address to the board and update our board address vars
        zorro2_auto_config_assign_base_address(board_low_addr);
        
        
        // assign the slot number
        if ((prev_config_flags & ZORRO_2_FLAG_NEXT_IS_RELATED) == 0) {
            slot++;
        }
        
        
        // add the board to the globals
        ExpansionBoard* board = &pSysDesc->expansion_board[pSysDesc->expansion_board_count++];
        board->start = board_low_addr;
        board->size = config.size;
        board->type = (isMemory) ? EXPANSION_TYPE_RAM : EXPANSION_TYPE_IO;
        board->bus = EXPANSION_BUS_ZORRO_2;
        board->slot = slot;
        board->manufacturer = config.manufacturer;
        board->product = config.product;
        board->serial_number = config.serial_number;
        
        prev_config_flags = config.flags;
        
        if (isMemory) {
            zorro_2_memory_expansion_addr = board_high_addr;
        } else {
            zorro_2_io_expansion_addr = board_high_addr;
        }
    }
}

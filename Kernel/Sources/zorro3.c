//
//  zorro3.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/10/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "SystemDescription.h"
#include "Bytes.h"
#include "Platform.h"


// Space for Zorro III auto configuration
#define ZORRO_3_CONFIG_BASE         ((Byte*)0xff000000)

// Space for Zorro III (memory and I/O) expansion boards
#define ZORRO_3_EXPANSION_LOW       ((Byte*)0x10000000)
#define ZORRO_3_EXPANSION_HIGH      ((Byte*)0x80000000)



// This board does not accept a shutup command
#define ZORRO_3_FLAG_CANT_SHUTUP        0x01
// This expansion entry is related to the next one. Eg both are part of teh same
// physical board (slot)
#define ZORRO_3_FLAG_NEXT_IS_RELATED    0x02

// This is a RA board (rather than an I/O board)
#define ZORRO_3_FLAG_IS_MEMORY          0x04


// Zorro III config info
typedef struct _Zorro3_Configuration {
    Byte*   start;          // base address
    UInt    physical_size;  // physical board size
    UInt    logical_size;   // logical board size which may be smaller than the physical size; 0 means the kernel should auto-size the board
    UInt16  flags;
    UInt16  manufacturer;
    UInt16  product;
    UInt32  serial_number;
} Zorro3_Configuration;



// Reads a byte value from the given Zorro III autoconfig address
static Byte zorro3_read(volatile Byte* addr, Bool invert)
{
    volatile Byte* pHigh8 = addr;
    volatile Byte* pLow8 = addr + 0x100;
    Byte byte = (pHigh8[0] & 0xf0) | ((pLow8[0] >> 4) & 0x0f);
    
    return (invert) ? ~byte : byte;
}

// Probes the autoconfig area for the presence of a expansion board. Returns true
// if a board was found and false otherwise. 'board' contains information about
// the board that was found. The contents of 'board' is undefined if no board was
// found.
// NOTE: We do not check whether cards actually return 0 for auto config locations
// for which they are supposed to retur 0 according to the spec because at least
// some cards do in fact return non-zero values. Eg Commodore A2091 SCSI card.
static Bool zorro3_read_config_space(Zorro3_Configuration* config)
{
    register Byte* pAutoConfigBase = ZORRO_3_CONFIG_BASE;
    
    // See: http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node02C7.html
    // See: http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node02C8.html
    Bytes_ClearRange((Byte*)config, sizeof(Zorro3_Configuration));
    
    
    // Type
    const UInt8 type = (UInt8)zorro3_read(pAutoConfigBase + 0x00, false);
    if ((type >> 6) != 2) {
        return false;
    }
    
    if (type & (1 << 5)) {
        config->flags |= ZORRO_3_FLAG_IS_MEMORY;
    }
    if (type & (1 << 3)) {
        config->flags |= ZORRO_3_FLAG_NEXT_IS_RELATED;
    }
    
    
    // Product
    config->product = (UInt16)zorro3_read(pAutoConfigBase + 0x04, true);
    
    
    // Flags
    const UInt8 flags = (UInt8)zorro3_read(pAutoConfigBase + 0x08, true);
    const Bool isExtendedSize = flags & (1 <<5);
    
    if ((flags & (1 << 4)) == 0) {
        return false;
    }
    if (flags & (1 << 6)) {
        config->flags |= ZORRO_3_FLAG_CANT_SHUTUP;
    }
    
    static const UInt gBoardSizes[8] = {
        SIZE_MB(8),   SIZE_KB(64), SIZE_KB(128), SIZE_KB(256),
        SIZE_KB(512), SIZE_MB(1),  SIZE_MB(2),   SIZE_MB(4)
    };
    static const UInt gExtendedBoardSizes[8] = {
        SIZE_MB(16),  SIZE_MB(32),  SIZE_MB(64), SIZE_MB(128),
        SIZE_MB(256), SIZE_MB(512), SIZE_GB(1),  0
    };
    
    config->physical_size = (isExtendedSize) ? gExtendedBoardSizes[type & 0x07] : gBoardSizes[type & 0x07];
    if (config->physical_size == 0) {
        return false;
    }
    
    static const UInt gLogicalSize[12] = {
        SIZE_KB(64), SIZE_KB(128), SIZE_KB(256), SIZE_KB(512),
        SIZE_MB(1),  SIZE_MB(2),   SIZE_MB(4),   SIZE_MB(6),
        SIZE_MB(8),  SIZE_MB(10),  SIZE_MB(12),  SIZE_MB(14)
    };
    
    const logsiz = flags & 0x0f;
    switch (logsiz) {
        case 0x0: config->logical_size = config->physical_size; // logical size is same as physical size
            break;
        case 0x1: config->logical_size = 0; // automatically sized by the kernel
            break;
        default:
            if (logsiz > 13) {
                return false;
            } else {
                config->logical_size = gLogicalSize[logsiz - 2];
            }
            break;
    }
    
    
    // Manufacturer
    const UInt8 manuHigh = (UInt8)zorro3_read(pAutoConfigBase + 0x10, true);
    const UInt8 manuLow = (UInt8)zorro3_read(pAutoConfigBase + 0x14, true);
    
    config->manufacturer = (manuHigh << 8) | manuLow;
    if (config->manufacturer == 0) {
        return false;
    }
    
    
    // Serial number
    const UInt8 sernum3 = (UInt8)zorro3_read(pAutoConfigBase + 0x18, true);
    const UInt8 sernum2 = (UInt8)zorro3_read(pAutoConfigBase + 0x1c, true);
    const UInt8 sernum1 = (UInt8)zorro3_read(pAutoConfigBase + 0x20, true);
    const UInt8 sernum0 = (UInt8)zorro3_read(pAutoConfigBase + 0x24, true);
    
    config->serial_number = (sernum3 << 24) | (sernum2 << 16) | (sernum1 << 8) | sernum0;
    
    
    // 0x28 & 0x2c -> optional ROM vector

    return true;
}

// Tells the board which is currently visible in the auto config space to
// shut up. This causes the bus to make teh next board in the chain available
// for configuration. The shut up board will enter idle state until the next
// system reset.
static void zorro3_auto_config_shutup(void)
{
    volatile Byte* pAddr = ((Byte*)(ZORRO_3_CONFIG_BASE + 0x4c));
    
    pAddr[0] = 0;
}

// Assigns the given address as the base address to the board currently visible
// in the auto config space. This moves the board to the new address and the next
// board becomes visible in auto config space.
static void zorro3_auto_config_assign_base_address(Byte* addr)
{
    UInt16 top16 = ((UInt) addr) >> 16;
    
    volatile Byte* pByte1 = ((Byte*)(ZORRO_3_CONFIG_BASE + 0x44));
    volatile Byte* pByte0  = ((Byte*)(ZORRO_3_CONFIG_BASE + 0x48));
    Byte byte1 = (top16 >> 8) & 0x00ff;
    Byte byte0  = (top16 >> 0) & 0x00ff;
    
    pByte0[0] = byte0;
    pByte1[0] = byte1;
}

// Dynamically determines the size of the given memory expansion board.
static UInt zorro3_auto_size_memory_board(Zorro3_Configuration* config)
{
    Byte* pLower = config->start;
    Byte* pUpper = config->start + config->physical_size;
    UInt size = 0;
    
    while (pLower < pUpper) {
        if (!mem_probe(pLower)) {
            break;
        }
        
        pLower += SIZE_KB(512);
        size += SIZE_KB(512);
    }
    
    return size;
}


void zorro3_auto_config(SystemDescription* pSysDesc)
{
    Byte* zorro_3_memory_expansion_addr = ZORRO_3_EXPANSION_LOW;
    Byte* zorro_3_io_expansion_addr = ZORRO_3_EXPANSION_HIGH;
    UInt8 prev_config_flags = ZORRO_3_FLAG_NEXT_IS_RELATED;
    Int slot = 0;
    
    pSysDesc->expansion_board_count = 0;
    
    while (pSysDesc->expansion_board_count < EXPANSION_BOARDS_CAPACITY) {
        Zorro3_Configuration config;
        
        const Bool isValid = zorro3_read_config_space(&config);
        if (!isValid) {
            break;
        }
        
        // compute the base address for the expansion board
        const Bool isMemory = (config.flags & ZORRO_3_FLAG_IS_MEMORY) != 0;
        const UInt board_size = align_uint(config.physical_size, SIZE_KB(64));
        Byte* board_low_addr;
        Byte* board_high_addr;
        
        // Zorro 3: unified mem and I/O space. Mem growing upward and I/O growing downward
        if (isMemory) {
            board_low_addr = zorro_3_memory_expansion_addr;
            board_high_addr = board_low_addr + board_size;
        } else {
            board_high_addr = zorro_3_io_expansion_addr;
            board_low_addr = board_high_addr - board_size;
        }
        
        
        // check whether we still got enough space left to map the board. If not then
        // shut the board up and move on to the next one.
        Byte* avail_low_addr;
        Byte* avail_high_addr;
        
        if (isMemory) {
            avail_low_addr = zorro_3_memory_expansion_addr;
            avail_high_addr = zorro_3_io_expansion_addr;
        } else {
            avail_low_addr = zorro_3_memory_expansion_addr;
            avail_high_addr = zorro_3_io_expansion_addr;
        }
        
        const Bool fits = (board_low_addr >= avail_low_addr && board_high_addr <= avail_high_addr);
        if (!fits) {
            // Have to stop looking for more boards if we can't shut this one up
            // because this means that we can't make the next board visible in the
            // config area...
            if ((config.flags & ZORRO_3_FLAG_CANT_SHUTUP) == 0) {
                zorro3_auto_config_shutup();
                continue;
            } else {
                break;
            }
        }
        
        
        // assign the start address
        zorro3_auto_config_assign_base_address(board_low_addr);
        
        
        // if RAM and logical_size == 0, auto-size the board
        if (config.logical_size == 0) {
            if (isMemory) {
                config.logical_size = zorro3_auto_size_memory_board(&config);
            } else {
                // This is really a hardware bug. Auto sizing for I/O boards makes no
                // sense 'cause there's no safe way to read / write registers blindly.
                config.logical_size = config.physical_size;
            }
        }
        
        
        // assign the slot number
        if ((prev_config_flags & ZORRO_3_FLAG_NEXT_IS_RELATED) == 0) {
            slot++;
        }
        
        
        // add the board to the globals
        ExpansionBoard* board = &pSysDesc->expansion_board[pSysDesc->expansion_board_count++];
        board->start = board_low_addr;
        board->size = config.logical_size;
        board->type = (isMemory) ? EXPANSION_TYPE_RAM : EXPANSION_TYPE_IO;
        board->bus = EXPANSION_BUS_ZORRO_3;
        board->slot = slot;
        board->manufacturer = config.manufacturer;
        board->product = config.product;
        board->serial_number = config.serial_number;
        
        prev_config_flags = config.flags;
        
        if (isMemory) {
            zorro_3_memory_expansion_addr = board_high_addr;
        } else {
            zorro_3_io_expansion_addr = board_low_addr;
        }
    }
}

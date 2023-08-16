//
//  zorro.c
//  Apollo
//
//  Created by Dietmar Planitzer on 4/27/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Platform.h"
#include "Bytes.h"
#include "SystemDescription.h"


// Space for Zorro II auto configuration
#define ZORRO_2_CONFIG_BASE         ((Byte*)0x00e80000)

// Space for Zorro III auto configuration
#define ZORRO_3_CONFIG_BASE         ((Byte*)0xff000000)


// Space for Zorro II memory expansion boards
#define ZORRO_2_MEMORY_LOW          ((Byte*)0x00200000)
#define ZORRO_2_MEMORY_HIGH         ((Byte*)0x00a00000)

// Space for Zorro II I/O expansion boards
#define ZORRO_2_IO_LOW              ((Byte*)0x00e90000)
#define ZORRO_2_IO_HIGH             ((Byte*)0x00f00000)

// Extra Space for Zorro II I/O expansion boards available in Zorro 3 machines
#define ZORRO_2_EXTRA_IO_LOW        ((Byte*)0x00a00000)
#define ZORRO_2_EXTRA_IO_HIGH       ((Byte*)0x00b80000)

// Space for Zorro III (memory and I/O) expansion boards
#define ZORRO_3_EXPANSION_LOW       ((Byte*)0x10000000)
#define ZORRO_3_EXPANSION_HIGH      ((Byte*)0x80000000)


// This board does not accept a shutup command
#define ZORRO_FLAG_CANT_SHUTUP      0x01

// This expansion entry is related to the next one. Eg both are part of the same
// physical board (slot)
#define ZORRO_FLAG_NEXT_IS_RELATED  0x02


// Zorro II config info
typedef struct _Zorro_BoardConfiguration {
    UInt    physical_size;  // physical board size
    UInt    logical_size;   // logical board size which may be smaller than the physical size; 0 means the kernel should auto-size the board
    UInt8   bus;
    UInt8   type;
    UInt8   flags;
    UInt8   reserved;
    UInt16  manufacturer;
    UInt16  product;
    UInt32  serial_number;
} Zorro_BoardConfiguration;



// Reads a byte value from the given Zorro auto configuration address
static Byte zorro_read(volatile Byte* _Nonnull addr, Bool invert, Bool isZorro3Machine)
{
    const UInt offset = (isZorro3Machine) ? 0x100 : 0x002;
    volatile Byte* pHigh8 = addr;
    volatile Byte* pLow8 = addr + offset;
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
static Bool zorro_read_config_space(Zorro_BoardConfiguration* _Nonnull config, UInt8 busToScan)
{
    const Bool isZorro3Machine = busToScan == EXPANSION_BUS_ZORRO_3;
    register Byte* pAutoConfigBase = (isZorro3Machine) ? ZORRO_3_CONFIG_BASE : ZORRO_2_CONFIG_BASE;
    
    // See: http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node02C7.html
    // See: http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node02C8.html
    Bytes_ClearRange((Byte*)config, sizeof(Zorro_BoardConfiguration));
    
    
    // Type
    const UInt8 type = (UInt8)zorro_read(pAutoConfigBase + 0x00, false, isZorro3Machine);
    switch((type >> 6) & 0x03) {
        case 0:     return false;
        case 1:     return false;
        case 2:     config->bus = EXPANSION_BUS_ZORRO_3; break;
        case 3:     config->bus = EXPANSION_BUS_ZORRO_2; break;
        default:    abort();
    }
    
    if (type & (1 << 5)) {
        config->type = EXPANSION_TYPE_RAM;
    } else {
        config->type = EXPANSION_TYPE_IO;
    }
    if (type & (1 << 3)) {
        config->flags |= ZORRO_FLAG_NEXT_IS_RELATED;
    }
    
    
    // Product
    config->product = (UInt16)zorro_read(pAutoConfigBase + 0x04, true, isZorro3Machine);
    
    
    // Flags
    const UInt8 flags = (UInt8)zorro_read(pAutoConfigBase + 0x08, true, isZorro3Machine);
    if (flags & (1 << 6)) {
        config->flags |= ZORRO_FLAG_CANT_SHUTUP;
    }
    
    const Bool isExtendedSize = (config->bus == EXPANSION_BUS_ZORRO_3) && (flags & (1 <<5)) != 0;
    const UInt8 physsiz = type & 0x07;
    static const UInt gBoardSizes[8] = {
        SIZE_MB(8),   SIZE_KB(64), SIZE_KB(128), SIZE_KB(256),
        SIZE_KB(512), SIZE_MB(1),  SIZE_MB(2),   SIZE_MB(4)
    };
    static const UInt gExtendedBoardSizes[8] = {
        SIZE_MB(16),  SIZE_MB(32),  SIZE_MB(64), SIZE_MB(128),
        SIZE_MB(256), SIZE_MB(512), SIZE_GB(1),  0
    };
    
    config->physical_size = (isExtendedSize) ? gExtendedBoardSizes[physsiz] : gBoardSizes[physsiz];
    if (config->physical_size == 0) {
        return false;
    }

    static const UInt gLogicalSize[12] = {
        SIZE_KB(64), SIZE_KB(128), SIZE_KB(256), SIZE_KB(512),
        SIZE_MB(1),  SIZE_MB(2),   SIZE_MB(4),   SIZE_MB(6),
        SIZE_MB(8),  SIZE_MB(10),  SIZE_MB(12),  SIZE_MB(14)
    };
    
    const UInt8 logsiz = (config->bus == EXPANSION_BUS_ZORRO_3) ? flags & 0x0f : 0;
    switch (logsiz) {
        case 0x00: config->logical_size = config->physical_size; // logical size is same as physical size
            break;
        case 0x01: config->logical_size = 0; // automatically sized by the kernel
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
    const UInt8 manuHigh = (UInt8)zorro_read(pAutoConfigBase + 0x10, true, isZorro3Machine);
    const UInt8 manuLow = (UInt8)zorro_read(pAutoConfigBase + 0x14, true, isZorro3Machine);
    
    config->manufacturer = (manuHigh << 8) | manuLow;
    if (config->manufacturer == 0) {
        return false;
    }
    
    
    // Serial number
    const UInt8 sernum3 = (UInt8)zorro_read(pAutoConfigBase + 0x18, true, isZorro3Machine);
    const UInt8 sernum2 = (UInt8)zorro_read(pAutoConfigBase + 0x1c, true, isZorro3Machine);
    const UInt8 sernum1 = (UInt8)zorro_read(pAutoConfigBase + 0x20, true, isZorro3Machine);
    const UInt8 sernum0 = (UInt8)zorro_read(pAutoConfigBase + 0x24, true, isZorro3Machine);
    
    config->serial_number = (sernum3 << 24) | (sernum2 << 16) | (sernum1 << 8) | sernum0;
    
    
    // 0x28 & 0x2c -> optional ROM vector
    
    return true;
}

static void zorro2_auto_config_shutup(void)
{
    volatile Byte* pNybble1 = ((Byte*)(ZORRO_2_CONFIG_BASE + 0x4c));
    volatile Byte* pNybble0 = ((Byte*)(ZORRO_2_CONFIG_BASE + 0x4e));
    
    pNybble0[0] = 0;
    pNybble1[0] = 0;
}

static void zorro3_auto_config_shutup(void)
{
    volatile Byte* pAddr = ((Byte*)(ZORRO_3_CONFIG_BASE + 0x4c));
    
    pAddr[0] = 0;
}

// Tells the board which is currently visible in the auto config space to
// shut up. This causes the bus to make teh next board in the chain available
// for configuration. The shut up board will enter idle state until the next
// system reset.
static void zorro_auto_config_shutup(UInt8 bus)
{
    if (bus == EXPANSION_BUS_ZORRO_3) {
        zorro3_auto_config_shutup();
    } else {
        zorro2_auto_config_shutup();
    }
}

static void zorro2_auto_config_assign_base_address(Byte* _Nullable addr)
{
    UInt16 top16 = ((UInt) addr) >> 16;
    
    volatile Byte* pNybble3 = ((Byte*)(ZORRO_2_CONFIG_BASE + 0x44));
    volatile Byte* pNybble2 = ((Byte*)(ZORRO_2_CONFIG_BASE + 0x46));
    volatile Byte* pNybble1  = ((Byte*)(ZORRO_2_CONFIG_BASE + 0x48));
    volatile Byte* pNybble0  = ((Byte*)(ZORRO_2_CONFIG_BASE + 0x4a));
    const Byte nybble3 = ((top16 >> 12) & 0x000f);
    const Byte nybble2 = ((top16 >>  8) & 0x000f);
    const Byte nybble1 = ((top16 >>  4) & 0x000f);
    const Byte nybble0 = ((top16 >>  0) & 0x000f);
    const Byte zNybble3 = (nybble3 << 4) | nybble3;
    const Byte zNybble2 = (nybble2 << 4) | nybble2;
    const Byte zNybble1 = (nybble1 << 4) | nybble1;
    const Byte zNybble0 = (nybble0 << 4) | nybble0;

    pNybble2[0] = zNybble2;
    pNybble3[0] = zNybble3;
    pNybble0[0] = zNybble0;
    pNybble1[0] = zNybble1;
}

static void zorro3_auto_config_assign_base_address(Byte* _Nullable addr)
{
    UInt16 top16 = ((UInt) addr) >> 16;
    
    volatile Byte* pByte1 = ((Byte*)(ZORRO_3_CONFIG_BASE + 0x44));
    volatile Byte* pByte0  = ((Byte*)(ZORRO_3_CONFIG_BASE + 0x48));
    Byte byte1 = (top16 >> 8) & 0x00ff;
    Byte byte0  = (top16 >> 0) & 0x00ff;
    
    pByte0[0] = byte0;
    pByte1[0] = byte1;
}

// Assigns the given address as the base address to the board currently visible
// in the auto config space. This moves the board to the new address and the next
// board becomes visible in auto config space.
static void zorro_auto_config_assign_base_address(Byte* _Nullable addr, UInt8 bus)
{
    if (bus == EXPANSION_BUS_ZORRO_3) {
        zorro3_auto_config_assign_base_address(addr);
    } else {
        zorro2_auto_config_assign_base_address(addr);
    }
}

static Byte* _Nonnull zorro2_align_board_address(Byte* _Nonnull base_ptr, UInt board_size, Bool isMemory)
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
        return align_up_byte_ptr(base_ptr, board_size);
    }
}

static Byte* _Nullable zorro_calculate_base_address_for_board_in_range(const Zorro_BoardConfiguration* _Nonnull pConfig, const ExpansionBus* _Nonnull pExpansionBus, Byte* board_space_base_addr, Byte* board_space_top_addr)
{
    const Bool isMemoryBoard = pConfig->type == EXPANSION_TYPE_RAM;
    const Bool isZorro3Board = pConfig->bus == EXPANSION_BUS_ZORRO_3;
    Byte* highest_occupied_board_addr = board_space_base_addr;
    const ExpansionBoard* pHighestAllocatedBoard = NULL;

    // Find the board with a matching Zorro bus, board type and expansion space adress range that has the highest assigned address
    for (Int i = 0; i < pExpansionBus->board_count; i++) {
        const ExpansionBoard* pCurBoard = &pExpansionBus->board[i];

        if (pCurBoard->bus == pConfig->bus
            && pCurBoard->type == pConfig->type
            && pCurBoard->start >= board_space_base_addr
            && pCurBoard->start < board_space_top_addr) {
            if (pCurBoard->start >= highest_occupied_board_addr) {
                highest_occupied_board_addr = pCurBoard->start;
                pHighestAllocatedBoard = pCurBoard;
            }
        }
    }


    // Calculate the address for the new board. It'll occupy the space just above the board we found.
    Byte* board_base_addr;
    if (pHighestAllocatedBoard != NULL) {
        Byte* proposed_board_base_addr = pHighestAllocatedBoard->start + pHighestAllocatedBoard->physical_size;

        if (isZorro3Board) {
            board_base_addr = align_up_byte_ptr(proposed_board_base_addr, pConfig->physical_size);
        } else {
            board_base_addr = zorro2_align_board_address(proposed_board_base_addr, pConfig->physical_size, isMemoryBoard);
        }
    } else {
        board_base_addr = board_space_base_addr;
    }
    Byte* board_top_addr = board_base_addr + pConfig->physical_size;
    
    return (board_top_addr <= board_space_top_addr) ? board_base_addr : NULL;
}

static Byte* _Nullable zorro_calculate_base_address_for_board(const Zorro_BoardConfiguration* _Nonnull pConfig, const ExpansionBus* _Nonnull pExpansionBus)
{
    Byte* board_base_addr = NULL;

    if (pConfig->bus == EXPANSION_BUS_ZORRO_3) {
        board_base_addr = zorro_calculate_base_address_for_board_in_range(pConfig, pExpansionBus, ZORRO_3_EXPANSION_LOW, ZORRO_3_EXPANSION_HIGH);
    } else {
        if (pConfig->type == EXPANSION_TYPE_RAM) {
            board_base_addr = zorro_calculate_base_address_for_board_in_range(pConfig, pExpansionBus, ZORRO_2_MEMORY_LOW, ZORRO_2_MEMORY_HIGH);
        } else {
            board_base_addr = zorro_calculate_base_address_for_board_in_range(pConfig, pExpansionBus, ZORRO_2_IO_LOW, ZORRO_2_IO_HIGH);
            if (board_base_addr == NULL && chipset_get_ramsey_version() > 0) {
                // Zorro 3 based machines support an extra Zorro 2 I/O address range
                board_base_addr = zorro_calculate_base_address_for_board_in_range(pConfig, pExpansionBus, ZORRO_2_EXTRA_IO_LOW, ZORRO_2_EXTRA_IO_HIGH);
            }
        }
    }

    return board_base_addr;
}

// Dynamically determines the size of the given memory expansion board.
static UInt zorro3_auto_size_memory_board(ExpansionBoard* _Nonnull board)
{
    Byte* pLower = board->start;
    Byte* pUpper = board->start + board->physical_size;
    UInt size = 0;
    
    while (pLower < pUpper) {
        if (cpu_verify_ram_4b(pLower)) {
            break;
        }
        
        pLower += SIZE_KB(512);
        size += SIZE_KB(512);
    }
    
    return size;
}



void zorro_auto_config(ExpansionBus* pExpansionBus)
{
    const Bool isZorro3Machine = chipset_get_ramsey_version() > 0;
    UInt8 prev_config_flags = ZORRO_FLAG_NEXT_IS_RELATED;
    Int slot = 0;
    
    pExpansionBus->board_count = 0;
    while (pExpansionBus->board_count < EXPANSION_BOARDS_CAPACITY) {
        Zorro_BoardConfiguration config;
        
        if (!zorro_read_config_space(&config, EXPANSION_BUS_ZORRO_2)) {
            if (!isZorro3Machine || (isZorro3Machine && !zorro_read_config_space(&config, EXPANSION_BUS_ZORRO_3))) {
                break;
            }
        }

        // calculate the base address for RAM or I/O. Growing bottom to top
        Byte* board_base_addr = zorro_calculate_base_address_for_board(&config, pExpansionBus);

        
        // check whether we still got enough space left to map the board. If not then
        // shut the board up and move on to the next one.
        if (board_base_addr == NULL) {
            // Have to stop looking for more boards if we can't shut this one up
            // because this means that we can't make the next board visible in the
            // config area...
            if ((config.flags & ZORRO_FLAG_CANT_SHUTUP) == 0) {
                zorro_auto_config_shutup(config.bus);
                continue;
            } else {
                break;
            }
        }
        
        
        // assign the start address to the board and update our board address vars
        zorro_auto_config_assign_base_address(board_base_addr, config.bus);

        
        // assign the slot number
        if ((prev_config_flags & ZORRO_FLAG_NEXT_IS_RELATED) == 0) {
            slot++;
        }
        
        
        // add the board to the globals
        ExpansionBoard* board = &pExpansionBus->board[pExpansionBus->board_count++];
        board->start = board_base_addr;
        board->physical_size = config.physical_size;
        board->logical_size = config.logical_size;
        board->type = config.type;
        board->bus = config.bus;
        board->slot = slot;
        board->manufacturer = config.manufacturer;
        board->product = config.product;
        board->serial_number = config.serial_number;
        

        // if RAM and logical_size == 0, auto-size the board
        if (board->logical_size == 0) {
            if (board->type == EXPANSION_TYPE_RAM) {
                board->logical_size = zorro3_auto_size_memory_board(board);
            } else {
                // This is really a hardware bug. Auto sizing for I/O boards makes no
                // sense 'cause there's no safe way to read / write registers blindly.
                board->logical_size = board->physical_size;
            }
        }


        prev_config_flags = config.flags;
    }
}

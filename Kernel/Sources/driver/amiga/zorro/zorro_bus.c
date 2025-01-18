//
//  zorro_bus.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/27/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "zorro_bus.h"
#include <klib/Assert.h>
#include <klib/Memory.h>
#include <hal/SystemDescription.h>


// Reads a byte value from the given Zorro auto configuration address
static uint8_t zorro_read(volatile uint8_t* _Nonnull addr, bool invert, bool isZorro3Machine)
{
    const size_t offset = (isZorro3Machine) ? 0x100 : 0x002;
    volatile uint8_t* pHigh8 = addr;
    volatile uint8_t* pLow8 = addr + offset;
    uint8_t byte = (pHigh8[0] & 0xf0) | ((pLow8[0] >> 4) & 0x0f);
    
    return (invert) ? ~byte : byte;
}

// Probes the autoconfig area for the presence of a expansion board. Returns true
// if a board was found and false otherwise. 'board' contains information about
// the board that was found. The contents of 'board' is undefined if no board was
// found.
// NOTE: We do not check whether cards actually return 0 for auto config locations
// for which they are supposed to return 0 according to the spec because at least
// some cards do in fact return non-zero values. Eg Commodore A2091 SCSI card.
static bool zorro_read_config_space(board_config_t* _Nonnull config, uint8_t busToScan)
{
    const bool isZorro3Machine = busToScan == ZORRO_3_BUS;
    register uint8_t* pAutoConfigBase = (isZorro3Machine) ? ZORRO_3_CONFIG_BASE : ZORRO_2_CONFIG_BASE;
    
    // See: http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node02C7.html
    // See: http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node02C8.html
    memset(config, 0, sizeof(board_config_t));
    
    
    // Type
    const uint8_t type = zorro_read(pAutoConfigBase + 0x00, false, isZorro3Machine);
    switch((type >> 6) & 0x03) {
        case 0:     return false;
        case 1:     return false;
        case 2:     config->bus = ZORRO_3_BUS; break;
        case 3:     config->bus = ZORRO_2_BUS; break;
        default:    abort();
    }
    
    if (type & (1 << 5)) {
        config->type = BOARD_TYPE_RAM;
    } else {
        config->type = BOARD_TYPE_IO;
    }
    if (type & (1 << 3)) {
        config->flags |= ZORRO_FLAG_NEXT_IS_RELATED;
    }
    
    
    // Product
    config->product = (uint16_t)zorro_read(pAutoConfigBase + 0x04, true, isZorro3Machine);
    
    
    // Flags
    const uint8_t flags = zorro_read(pAutoConfigBase + 0x08, true, isZorro3Machine);
    if (flags & (1 << 6)) {
        config->flags |= ZORRO_FLAG_CANT_SHUTUP;
    }
    
    const bool isExtendedSize = (config->bus == ZORRO_3_BUS) && (flags & (1 <<5)) != 0;
    const uint8_t physsiz = type & 0x07;
    static const size_t gBoardSizes[8] = {
        SIZE_MB(8),   SIZE_KB(64), SIZE_KB(128), SIZE_KB(256),
        SIZE_KB(512), SIZE_MB(1),  SIZE_MB(2),   SIZE_MB(4)
    };
    static const size_t gExtendedBoardSizes[8] = {
        SIZE_MB(16),  SIZE_MB(32),  SIZE_MB(64), SIZE_MB(128),
        SIZE_MB(256), SIZE_MB(512), SIZE_GB(1),  0
    };
    
    config->physical_size = (isExtendedSize) ? gExtendedBoardSizes[physsiz] : gBoardSizes[physsiz];
    if (config->physical_size == 0) {
        return false;
    }

    static const size_t gLogicalSize[12] = {
        SIZE_KB(64), SIZE_KB(128), SIZE_KB(256), SIZE_KB(512),
        SIZE_MB(1),  SIZE_MB(2),   SIZE_MB(4),   SIZE_MB(6),
        SIZE_MB(8),  SIZE_MB(10),  SIZE_MB(12),  SIZE_MB(14)
    };
    
    const uint8_t logsiz = (config->bus == ZORRO_3_BUS) ? flags & 0x0f : 0;
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
    const uint8_t manuHigh = zorro_read(pAutoConfigBase + 0x10, true, isZorro3Machine);
    const uint8_t manuLow = zorro_read(pAutoConfigBase + 0x14, true, isZorro3Machine);
    
    config->manufacturer = (manuHigh << 8) | manuLow;
    if (config->manufacturer == 0) {
        return false;
    }
    
    
    // Serial number
    const uint8_t sernum3 = zorro_read(pAutoConfigBase + 0x18, true, isZorro3Machine);
    const uint8_t sernum2 = zorro_read(pAutoConfigBase + 0x1c, true, isZorro3Machine);
    const uint8_t sernum1 = zorro_read(pAutoConfigBase + 0x20, true, isZorro3Machine);
    const uint8_t sernum0 = zorro_read(pAutoConfigBase + 0x24, true, isZorro3Machine);
    
    config->serial_number = (sernum3 << 24) | (sernum2 << 16) | (sernum1 << 8) | sernum0;
    
    
    // 0x28 & 0x2c -> optional ROM vector
    
    return true;
}

static void zorro2_auto_config_shutup(void)
{
    volatile uint8_t* pNybble1 = ((uint8_t*)(ZORRO_2_CONFIG_BASE + 0x4c));
    volatile uint8_t* pNybble0 = ((uint8_t*)(ZORRO_2_CONFIG_BASE + 0x4e));
    
    pNybble0[0] = 0;
    pNybble1[0] = 0;
}

static void zorro3_auto_config_shutup(void)
{
    volatile uint8_t* pAddr = ((uint8_t*)(ZORRO_3_CONFIG_BASE + 0x4c));
    
    pAddr[0] = 0;
}

// Tells the board which is currently visible in the auto config space to
// shut up. This causes the bus to make the next board in the chain available
// for configuration. The shut up board will enter idle state until the next
// system reset.
static void zorro_auto_config_shutup(uint8_t bus)
{
    if (bus == ZORRO_3_BUS) {
        zorro3_auto_config_shutup();
    } else {
        zorro2_auto_config_shutup();
    }
}

static void zorro2_auto_config_assign_base_address(uint8_t* _Nullable addr)
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

static void zorro3_auto_config_assign_base_address(uint8_t* _Nullable addr)
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
static void zorro_auto_config_assign_base_address(uint8_t* _Nullable addr, uint8_t bus)
{
    if (bus == ZORRO_3_BUS) {
        zorro3_auto_config_assign_base_address(addr);
    } else {
        zorro2_auto_config_assign_base_address(addr);
    }
}

static uint8_t* _Nonnull zorro2_align_board_address(uint8_t* _Nonnull base_ptr, unsigned int board_size, bool isMemory)
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

static uint8_t* _Nullable zorro_calculate_base_address_for_board_in_range(const board_config_t* _Nonnull pConfig, const zorro_bus_t* _Nonnull bus, uint8_t* board_space_base_addr, uint8_t* board_space_top_addr)
{
    const bool isMemoryBoard = pConfig->type == BOARD_TYPE_RAM;
    const bool isZorro3Board = pConfig->bus == ZORRO_3_BUS;
    uint8_t* highest_occupied_board_addr = board_space_base_addr;
    const zorro_board_t* pHighestAllocatedBoard = NULL;

    // Find the board with a matching Zorro bus, board type and expansion space adress range that has the highest assigned address
    for (size_t i = 0; i < bus->count; i++) {
        const zorro_board_t* pCurBoard = &bus->board[i];

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
    uint8_t* board_base_addr;
    if (pHighestAllocatedBoard != NULL) {
        uint8_t* proposed_board_base_addr = pHighestAllocatedBoard->start + pHighestAllocatedBoard->physical_size;

        if (isZorro3Board) {
            board_base_addr = __Ceil_Ptr_PowerOf2(proposed_board_base_addr, pConfig->physical_size);
        } else {
            board_base_addr = zorro2_align_board_address(proposed_board_base_addr, pConfig->physical_size, isMemoryBoard);
        }
    } else {
        board_base_addr = board_space_base_addr;
    }
    uint8_t* board_top_addr = board_base_addr + pConfig->physical_size;
    
    return (board_top_addr <= board_space_top_addr) ? board_base_addr : NULL;
}

static uint8_t* _Nullable zorro_calculate_base_address_for_board(const board_config_t* _Nonnull pConfig, const zorro_bus_t* _Nonnull bus)
{
    uint8_t* board_base_addr = NULL;

    if (pConfig->bus == ZORRO_3_BUS) {
        board_base_addr = zorro_calculate_base_address_for_board_in_range(pConfig, bus, ZORRO_3_EXPANSION_LOW, ZORRO_3_EXPANSION_HIGH);
    } else {
        if (pConfig->type == BOARD_TYPE_RAM) {
            board_base_addr = zorro_calculate_base_address_for_board_in_range(pConfig, bus, ZORRO_2_MEMORY_LOW, ZORRO_2_MEMORY_HIGH);
        } else {
            board_base_addr = zorro_calculate_base_address_for_board_in_range(pConfig, bus, ZORRO_2_IO_LOW, ZORRO_2_IO_HIGH);
            if (board_base_addr == NULL && chipset_get_ramsey_version() > 0) {
                // Zorro 3 based machines support an extra Zorro 2 I/O address range
                board_base_addr = zorro_calculate_base_address_for_board_in_range(pConfig, bus, ZORRO_2_EXTRA_IO_LOW, ZORRO_2_EXTRA_IO_HIGH);
            }
        }
    }

    return board_base_addr;
}

// Dynamically determines the size of the given memory expansion board.
static size_t zorro3_auto_size_memory_board(zorro_board_t* _Nonnull board)
{
    uint8_t* pLower = board->start;
    uint8_t* pUpper = board->start + board->physical_size;
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



void zorro_auto_config(zorro_bus_t* _Nonnull bus)
{
    const bool isZorro3Machine = chipset_get_ramsey_version() > 0;
    uint8_t prev_config_flags = ZORRO_FLAG_NEXT_IS_RELATED;
    int slot = 0;
    
    bus->count = 0;
    while (bus->count < ZORRO_BUS_CAPACITY) {
        board_config_t config;
        
        if (!zorro_read_config_space(&config, ZORRO_2_BUS)) {
            if (!isZorro3Machine || (isZorro3Machine && !zorro_read_config_space(&config, ZORRO_3_BUS))) {
                break;
            }
        }

        // calculate the base address for RAM or I/O. Growing bottom to top
        uint8_t* board_base_addr = zorro_calculate_base_address_for_board(&config, bus);

        
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
        zorro_board_t* board = &bus->board[bus->count++];
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
            if (board->type == BOARD_TYPE_RAM) {
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

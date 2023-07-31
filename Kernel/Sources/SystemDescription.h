//
//  SystemDescription.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef SystemDescription_h
#define SystemDescription_h

#include "Foundation.h"


// Supported max number of memory descriptors
#define MEMORY_DESCRIPTORS_CAPACITY  8

// Supported max number of expansion boards
#define EXPANSION_BOARDS_CAPACITY    16


// Specifies who can access a specific memory range
#define MEM_ACCESS_CPU     1
#define MEM_ACCESS_CHIPSET 2

// A memory descriptor describes a contiguous range of RAM
typedef struct _MemoryDescriptor {
    Byte* _Nonnull  lower;
    Byte* _Nonnull  upper;
    UInt8           accessibility;      // MEM_ACCESS_XXX
    UInt8           reserved[3];
    // Update lowmem.i if you add a new property here
} MemoryDescriptor;

typedef struct _MemoryLayout {
    Int                 descriptor_count;
    MemoryDescriptor    descriptor[MEMORY_DESCRIPTORS_CAPACITY];
} MemoryLayout;


// Expanion board types
#define EXPANSION_TYPE_RAM  0
#define EXPANSION_TYPE_IO   1

// Expansion bus types
#define EXPANSION_BUS_ZORRO_2   0
#define EXPANSION_BUS_ZORRO_3   1

// An expansion board
typedef struct _ExpansionBoard {
    Byte* _Nonnull  start;          // base address
    UInt            physical_size;  // size of memory space reserved for this board
    UInt            logical_size;   // size of memory space actually occupied by the board
    Int8            type;
    Int8            bus;
    Int8            slot;
    Int8            reserved;
    UInt16          manufacturer;
    UInt16          product;
    UInt32          serial_number;
    // Update lowmem.i if you add a new property here
} ExpansionBoard;

typedef struct _ExpansionBus {
    Int                 board_count;
    ExpansionBoard      board[EXPANSION_BOARDS_CAPACITY];
} ExpansionBus;

// The system description
// Note: Keep in sync with lowmem.i
typedef struct _SystemDescription {
    Byte* _Nonnull      stack_base;     // Reset / Kernel stack base pointer
    Int                 stack_size;
    Int8                cpu_model;
    Int8                fpu_model;
    // All fields above this point are filled in before OnReset() is called.
    // All fields below this point are initialized by the SystemDescription_Init() function
    UInt8               chipset_version;
    UInt8               chipset_ramsey_version;     // RAMSEY custom chip version. 0 means no RAMSEY and thus a 16bit Amiga (A500 / A2000)
    
    Int32               quantum_duration_ns;        // Quantum duration in terms of nenoseconds
    Int16               quantum_duration_cycles;    // Quantum duration in terms of timer cycles
    Int16               ns_per_quantum_timer_cycle; // Lenght of a quantu timer cycle in nanoseconds
    
    MemoryLayout        memory;
    ExpansionBus        expansion;
} SystemDescription;


// Returns a reference to the shared system description.
extern SystemDescription* _Nonnull SystemDescription_GetShared(void);

extern void SystemDescription_Init(SystemDescription* _Nonnull pSysDesc);

#endif /* SystemDescription_h */

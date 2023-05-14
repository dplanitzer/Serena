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


// CPU types
#define CPU_MODEL_68000     0
#define CPU_MODEL_68010     1
#define CPU_MODEL_68020     2
#define CPU_MODEL_68030     3
#define CPU_MODEL_68040     4
#define CPU_MODEL_68060     6

// FPU types
#define FPU_MODEL_NONE      0
#define FPU_MODEL_68881     1
#define FPU_MODEL_68882     2
#define FPU_MODEL_68040     3
#define FPU_MODEL_68060     4


// 8361 (Regular) or 8370 (Fat) (Agnus-NTSC) = 10, 512KB
// 8367 (Pal) or 8371 (Fat-Pal) (Agnus-PAL) = 00, 512KB
// 8372 (Fat-hr) (agnushr),thru rev4 = 20 PAL, 30 NTSC, 1MB
// 8372 (Fat-hr) (agnushr),rev 5 = 22 PAL, 31 NTSC, 1MB
// 8374 (Alice) thru rev 2 = 22 PAL, 32 NTSC, 2MB
// 8374 (Alice) rev 3 thru rev 4 = 23 PAL, 33 NTSC, 2MB
#define CHIPSET_8361_NTSC       0x10
#define CHIPSET_8367_PAL        0x00
#define CHIPSET_8370_NTSC       0x10
#define CHIPSET_8371_PAL        0x00
#define CHIPSET_8372_rev4_PAL   0x20
#define CHIPSET_8372_rev4_NTSC  0x30
#define CHIPSET_8372_rev5_PAL   0x22
#define CHIPSET_8372_rev5_NTSC  0x31
#define CHIPSET_8374_rev2_PAL   0x22
#define CHIPSET_8374_rev2_NTSC  0x32
#define CHIPSET_8374_rev3_PAL   0x23
#define CHIPSET_8374_rev3_NTSC  0x33


// RAMSEY chip versions (32bit Amigas only. Like A3000 / A4000)
#define CHIPSET_RAMSEY_rev04    0x0d
#define CHIPSET_RAMSEY_rev07    0x0f


// Supported max number of memory descriptors
#define MEMORY_DESCRIPTORS_CAPACITY  8

// Supported max number of expansion boards
#define EXPANSION_BOARDS_CAPACITY    16


// The system description
// Note: Keep in sync with lowmem.i
typedef struct _SystemDescription {
    Byte* _Nonnull      stack_base;     // Reset / Kernel stack base pointer
    Int                 stack_size;
    Int8                cpu_model;
    Int8                fpu_model;
    // All fields above this point are filled in before OnReset() is called.
    // All fields below this point are initialized by the SystemDescription_Init() function
    Int8                chipset_version;
    Int8                chipset_ramsey_version;     // RAMSEY custom chip version. 0 means no RAMSEY and thus a 16bit Amiga (A500 / A2000)
    
    Int32               quantum_duration_ns;        // Quantum duration in terms of nenoseconds
    Int16               quantum_duration_cycles;    // Quantum duration in terms of timer cycles
    Int16               ns_per_quantum_timer_cycle; // Lenght of a quantu timer cycle in nanoseconds
    
    Int                 memory_descriptor_count;
    MemoryDescriptor    memory_descriptor[MEMORY_DESCRIPTORS_CAPACITY];
    
    Int                 expansion_board_count;
    ExpansionBoard      expansion_board[EXPANSION_BOARDS_CAPACITY];
} SystemDescription;


// Returns a reference to the shared system description.
extern SystemDescription* _Nonnull SystemDescription_GetShared(void);

extern void SystemDescription_Init(SystemDescription* _Nonnull pSysDesc);

#endif /* SystemDescription_h */

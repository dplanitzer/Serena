//
//  Platform.h
//  Kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Platform_h
#define Platform_h

#include "Foundation.h"

//
// CPU
//

// Size of a standard page in bytes
#define CPU_PAGE_SIZE   4096


#if __LP64__
#define STACK_ALIGNMENT  16
#elif __LP32__
#define STACK_ALIGNMENT  4
#else
#error "don't know how to align stack pointers"
#endif


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


// CPU register state
typedef struct _CpuContext {
    // 68000 or better
    UInt32      d[8];
    UInt32      a[8];
    UInt32      usp;
    UInt32      pc;
    UInt16      sr;
    UInt16      padding;
    // 68881, 68882, 68040 or better
    UInt32      fsave[54];  // fsave / frestore data (see M68000PRM p. 6-12)
    Float96     fp[8];
    UInt32      fpcr;
    UInt32      fpsr;
    UInt32      fpiar;
} CpuContext;


typedef void (* _Nonnull Cpu_UserClosure)(Byte* _Nullable pContext);


extern void cpu_enable_irqs(void);
extern Int cpu_disable_irqs(void);
extern void cpu_restore_irqs(Int state);
extern void cpu_set_irq_stack_pointer(Byte* pStackPtr);

extern const Character* _Nonnull cpu_get_model_name(Int8 cpu_model);

extern Int cpu_guarded_read(Byte* _Nonnull src, Byte* _Nonnull buffer, Int buffer_size);
extern Int cpu_guarded_write(Byte* _Nonnull dst, const Byte* _Nonnull buffer, Int buffer_size);

extern void cpu_sleep(Int cpu_type);

extern void cpu_call_as_user(Cpu_UserClosure _Nonnull pClosure, Byte* _Nullable pContext);
extern void cpu_abort_call_as_user(void);

extern _Noreturn cpu_non_recoverable_error(void);
extern _Noreturn mem_non_recoverable_error(void);


//
// FPU
//
extern const Character* _Nonnull fpu_get_model_name(Int8 fpu_model);


//
// Memory
//

// Supported max number of memory descriptors
#define MEMORY_DESCRIPTORS_CAPACITY  8

// The type of memory
// Memory accessible to the CPU only
#define MEM_TYPE_MEMORY         0
// Memory accessible to the CPU and I/O (GPU, Audio, etc)
#define MEM_TYPE_UNIFIED_MEMORY 1

// A memory descriptor describes a contiguous range of RAM
typedef struct _MemoryDescriptor {
    Byte* _Nonnull  lower;
    Byte* _Nonnull  upper;
    Int8            type;       // MEM_TYPE_XXX
    UInt8           reserved[3];
} MemoryDescriptor;

typedef struct _MemoryLayout {
    Int                 descriptor_count;
    MemoryDescriptor    descriptor[MEMORY_DESCRIPTORS_CAPACITY];
} MemoryLayout;


extern Bool mem_probe(Byte* _Nonnull addr);
extern Bool mem_check_region(MemoryLayout* pMemLayout, Byte* lower, Byte* upper, Int8 type);


#define CIAA_BASE           0xbfe001
#define CIAB_BASE           0xbfd000
#define RTC_BASE            0xdc0000
#define RAMSEY_CHIP_BASE    0xde0043
#define CUSTOM_BASE         0xdff000
#define DIAGNOSTIC_ROM_BASE 0xf00000
#define DIAGNOSTIC_ROM_SIZE 0x80000
#define EXT_ROM_BASE        0xf80000
#define EXT_ROM_SIZE        0x40000
#define BOOT_ROM_BASE       0xfc0000
#define BOOT_ROM_SIZE       0x40000


//
// Chipset
//


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


// Chipset registers
#define VPOSR       0x004
#define DIWSTART    0x08e
#define DIWSTOP     0x090
#define DDFSTART    0x092
#define DDFSTOP     0x094
#define DMACON      0x096
#define BPL1PTH     0x0e0
#define BPL1PTL     0x0e2
#define BPL2PTH     0x0e4
#define BPL2PTL     0x0e6
#define BPL3PTH     0x0e8
#define BPL3PTL     0x0ea
#define BPL4PTH     0x0ec
#define BPL4PTL     0x0ee
#define BPL5PTH     0x0f0
#define BPL5PTL     0x0f2
#define BPL6PTH     0x0f4
#define BPL6PTL     0x0f6

#define COP1LCH     0x080
#define COP1LCL     0x082
#define COP2LCH     0x084
#define COP2LCL     0x086
#define COPJMP1     0x088
#define COPJMP2     0x08A

#define BPLCON0     0x100
#define BPLCON1     0x102
#define BPLCON2     0x104
#define BPL1MOD     0x108
#define BPL2MOD     0x10a

#define SPR0PTH     0x120
#define SPR0PTL     0x122
#define SPR1PTH     0x124
#define SPR1PTL     0x126
#define SPR2PTH     0x128
#define SPR2PTL     0x12a
#define SPR3PTH     0x12c
#define SPR3PTL     0x12e
#define SPR4PTH     0x130
#define SPR4PTL     0x132
#define SPR5PTH     0x134
#define SPR5PTL     0x136
#define SPR6PTH     0x138
#define SPR6PTL     0x13a
#define SPR7PTH     0x13c
#define SPR7PTL     0x13e

#define COLOR_BASE  0x180
#define COLOR00     COLOR_BASE+0x00
#define COLOR01     COLOR_BASE+0x02
#define COLOR02     COLOR_BASE+0x04
#define COLOR03     COLOR_BASE+0x06
#define COLOR04     COLOR_BASE+0x08
#define COLOR05     COLOR_BASE+0x0A
#define COLOR06     COLOR_BASE+0x0C
#define COLOR07     COLOR_BASE+0x0E
#define COLOR08     COLOR_BASE+0x10
#define COLOR09     COLOR_BASE+0x12
#define COLOR10     COLOR_BASE+0x14
#define COLOR11     COLOR_BASE+0x16
#define COLOR12     COLOR_BASE+0x18
#define COLOR13     COLOR_BASE+0x1A
#define COLOR14     COLOR_BASE+0x1C
#define COLOR15     COLOR_BASE+0x1E
#define COLOR16     COLOR_BASE+0x20
#define COLOR17     COLOR_BASE+0x22
#define COLOR18     COLOR_BASE+0x24
#define COLOR19     COLOR_BASE+0x26
#define COLOR20     COLOR_BASE+0x28
#define COLOR21     COLOR_BASE+0x2A
#define COLOR22     COLOR_BASE+0x2C
#define COLOR23     COLOR_BASE+0x2E
#define COLOR24     COLOR_BASE+0x30
#define COLOR25     COLOR_BASE+0x32
#define COLOR26     COLOR_BASE+0x34
#define COLOR27     COLOR_BASE+0x36
#define COLOR28     COLOR_BASE+0x38
#define COLOR29     COLOR_BASE+0x3A
#define COLOR30     COLOR_BASE+0x3C
#define COLOR31     COLOR_BASE+0x3E

#define BPLCON0_LACE    0x0004


// Copper instructions
typedef UInt32  CopperInstruction;

#define COP_MOVE(reg, val)  (((reg) << 16) | (val))
#define COP_END()           0xfffffffe


// The supported interrupts
// http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0142.html
// http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0036.html
#define INTERRUPT_ID_CIA_B_FLAG                     23
#define INTERRUPT_ID_CIA_B_SP                       22
#define INTERRUPT_ID_CIA_B_ALARM                    21
#define INTERRUPT_ID_CIA_B_TIMER_B                  20
#define INTERRUPT_ID_CIA_B_TIMER_A                  19

#define INTERRUPT_ID_CIA_A_FLAG                     18
#define INTERRUPT_ID_CIA_A_SP                       17
#define INTERRUPT_ID_CIA_A_ALARM                    16
#define INTERRUPT_ID_CIA_A_TIMER_B                  15
#define INTERRUPT_ID_CIA_A_TIMER_A                  14

#define INTERRUPT_ID_EXTERN                         13
#define INTERRUPT_ID_DISK_SYNC                      12
#define INTERRUPT_ID_SERIAL_RECEIVE_BUFFER_FULL     11
#define INTERRUPT_ID_AUDIO3                         10
#define INTERRUPT_ID_AUDIO2                         9
#define INTERRUPT_ID_AUDIO1                         8
#define INTERRUPT_ID_AUDIO0                         7
#define INTERRUPT_ID_BLITTER                        6
#define INTERRUPT_ID_VERTICAL_BLANK                 5
#define INTERRUPT_ID_COPPER                         4
#define INTERRUPT_ID_PORTS                          3
#define INTERRUPT_ID_SOFT                           2
#define INTERRUPT_ID_DISK_BLOCK                     1
#define INTERRUPT_ID_SERIAL_TRANSMIT_BUFFER_EMPTY   0

#define INTERRUPT_ID_COUNT                          24


extern void chipset_reset(void);
extern UInt8 chipset_get_version(void);
extern UInt8 chipset_get_ramsey_version(void);
extern Bool chipset_is_ntsc(void);
extern Byte* chipset_get_upper_dma_limit(Int chipset_version);

extern void chipset_enable_interrupt(Int interruptId);
extern void chipset_disable_interrupt(Int interruptId);


#define INTERRUPT_ID_QUANTUM_TIMER  INTERRUPT_ID_CIA_A_TIMER_B

extern void chipset_start_quantum_timer(void);
extern void chipset_stop_quantum_timer(void);
extern Int32 chipset_get_quantum_timer_duration_ns(void);
extern Int32 chipset_get_quantum_timer_elapsed_ns(void);


//
// Copper
//

#define COP_FLAG_SCHEDULED  (1 << 7)
#define COP_FLAG_INTERLACED (1 << 6)

// Keep in sync with lowmem.i
typedef struct _CopperScheduler {
    UInt32                          flags;
    CopperInstruction* _Nullable    scheduled_prog_odd_field;
    CopperInstruction* _Nullable    scheduled_prog_even_field;
    Int                             scheduled_prog_id;
    CopperInstruction* _Nullable    running_prog_odd_field;
    CopperInstruction* _Nullable    running_prog_even_field;
    Int                             running_prog_id;
} CopperScheduler;

extern CopperScheduler  gCopperSchedulerStorage;

extern void copper_schedule_program(const CopperInstruction* _Nullable pOddFieldProg, const CopperInstruction* _Nullable pEvenFieldProg, Int progId);
extern Int copper_get_running_program_id(void);


//
// Zorro Bus
//

// Supported max number of expansion boards
#define EXPANSION_BOARDS_CAPACITY    16

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

extern void zorro_auto_config(ExpansionBus* pExpansionBus);

#endif /* Platform_h */

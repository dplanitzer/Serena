//
//  m68k/cpu.h
//  Kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _CPU_M68K_H
#define _CPU_M68K_H 1

#include <kern/floattypes.h>
#include <kern/types.h>

// Size of a standard page in bytes
#define CPU_PAGE_SIZE   4096

#define STACK_ALIGNMENT  4


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


// FPU state frame sizes (fsave/frestore, see M68000PRM p. 6-12)
#define FPU_NULL_STATE_SIZE         4
#define FPU_68040_IDLE_STATE_SIZE   4
#define FPU_68881_IDLE_STATE_SIZE   28
#define FPU_68882_IDLE_STATE_SIZE   60
#define FPU_68040_UNIMP_STATE_SIZE  48
#define FPU_68040_BUSY_STATE_SIZE   96
#define FPU_68881_BUSY_STATE_SIZE   184
#define FPU_68882_BUSY_STATE_SIZE   216
#define FPU_MAX_STATE_SIZE          216


// CPU (68k) address space selector (Alternate function codes)
#define CPU68K_USER_DATA_SPACE  1
#define CPU68K_USER_CODE_SPACE  2
#define CPU68K_SUPER_DATA_SPACE 5
#define CPU68K_SUPER_CODE_SPACE 6
#define CPU68K_CPU_SPACE        7


// Status register
#define CPU_SR_TRACE        0x8000
#define CPU_SR_S            0x2000
#define CPU_SR_IE_MASK      0x700
#define CPU_SR_X            0x10
#define CPU_SR_N            0x08
#define CPU_SR_Z            0x04
#define CPU_SR_V            0x02
#define CPU_SR_C            0x01


// CPU register state (keep in sync with lowmem.i)
typedef struct mcontext {
    
    // Integer state. 68000 or better
    uint32_t    d[8];
    uintptr_t   a[8];
    uintptr_t   usp;
    uintptr_t   pc;
    uint16_t    sr;
    uint16_t    padding;

    // Floating-point state. 68881, 68882, 68040 or better
    uint8_t     fsave[FPU_MAX_STATE_SIZE];  // fsave / frestore data
    float96_t   fp[8];
    uint32_t    fpcr;
    uint32_t    fpsr;
    uint32_t    fpiar;
} mcontext_t;


#define EXCPT_OFFSET_RESET_SSP  0
#define EXCPT_OFFSET_RESET_PC   4
#define EXCPT_OFFSET_BUS_ERR    8
#define EXCPT_OFFSET_ADR_ERR    12
#define EXCPT_OFFSET_ILL_INSTR  16
#define EXCPT_OFFSET_ZERO_DIV   20
#define EXCPT_OFFSET_CHK        24
#define EXCPT_OFFSET_TRAPX      28
#define EXCPT_OFFSET_PRIV_VIO   32
#define EXCPT_OFFSET_TRACE      36
#define EXCPT_OFFSET_LINE_A     40
#define EXCPT_OFFSET_LINE_F     44
#define EXCPT_OFFSET_EMU        48
#define EXCPT_OFFSET_COPROC     52
#define EXCPT_OFFSET_FORMAT     56
#define EXCPT_OFFSET_UNINIT_IRQ 60
#define EXCPT_OFFSET_RESV_16    64
#define EXCPT_OFFSET_RESV_17    68
#define EXCPT_OFFSET_RESV_18    72
#define EXCPT_OFFSET_RESV_19    76
#define EXCPT_OFFSET_RESV_20    80
#define EXCPT_OFFSET_RESV_21    84
#define EXCPT_OFFSET_RESV_22    88
#define EXCPT_OFFSET_RESV_23    92
#define EXCPT_OFFSET_IRQ_1      100
#define EXCPT_OFFSET_IRQ_2      104
#define EXCPT_OFFSET_IRQ_3      108
#define EXCPT_OFFSET_IRQ_4      112
#define EXCPT_OFFSET_IRQ_5      116
#define EXCPT_OFFSET_IRQ_6      120
#define EXCPT_OFFSET_IRQ_7      124
#define EXCPT_OFFSET_TRAP_0     128
#define EXCPT_OFFSET_TRAP_1     132
#define EXCPT_OFFSET_TRAP_2     136
#define EXCPT_OFFSET_TRAP_3     140
#define EXCPT_OFFSET_TRAP_4     144
#define EXCPT_OFFSET_TRAP_5     148
#define EXCPT_OFFSET_TRAP_6     152
#define EXCPT_OFFSET_TRAP_7     156
#define EXCPT_OFFSET_TRAP_8     160
#define EXCPT_OFFSET_TRAP_9     164
#define EXCPT_OFFSET_TRAP_10    168
#define EXCPT_OFFSET_TRAP_11    172
#define EXCPT_OFFSET_TRAP_12    176
#define EXCPT_OFFSET_TRAP_13    180
#define EXCPT_OFFSET_TRAP_14    184
#define EXCPT_OFFSET_TRAP_15    188
#define EXCPT_OFFSET_FPU_BR_UO      192
#define EXCPT_OFFSET_FPU_INEXACT    196
#define EXCPT_OFFSET_FPU_DIV_ZERO   200
#define EXCPT_OFFSET_FPU_UNDERFLOW  204
#define EXCPT_OFFSET_FPU_OP_ERR     208
#define EXCPT_OFFSET_FPU_OVERFLOW   212
#define EXCPT_OFFSET_FPU_SNAN       216
#define EXCPT_OFFSET_FPU_UNIMPL_TY  220
#define EXCPT_OFFSET_MMU_CONF_ERR   224
#define EXCPT_OFFSET_MMU_ILL_OP     228
#define EXCPT_OFFSET_MMU_ACCESS_VIO 232
#define EXCPT_OFFSET_RESV_59    236
#define EXCPT_OFFSET_UNIMPL_EA  240
#define EXCPT_OFFSET_UNIMPL_INT 244
#define EXCPT_OFFSET_RESV_62    248
#define EXCPT_OFFSET_RESV_63    252
#define EXCPT_OFFSET_USER_VEC   256

#define EXCPT_NUM_USER_VECS     192


// CPU exception stack frame
// 68020UM, p6-27
#pragma pack(1)
typedef struct excpt_frame {
    uint16_t    sr;
    uintptr_t   pc;
    uint16_t    fv;
    
    union {
        struct Format2 {
            uintptr_t   addr;
        }   f2;
        struct Format3 {
            uintptr_t   ea;
        }   f3;
        struct Format4 {
            uintptr_t   ea;
            uintptr_t   pcFaultedInstr;
        }   f4;
        struct Format7 {
            uintptr_t   ea;
            uint16_t    ssw;
            uint16_t    wb3s;
            uint16_t    wb2s;
            uint16_t    wb1s;
            uint32_t    fa;
            uint32_t    wb3a;
            uint32_t    wb3d;
            uint32_t    wb2a;
            uint32_t    wb2d;
            uint32_t    wb1a;
            uint32_t    wb1d;
            uint32_t    pd1;
            uint32_t    pd2;
            uint32_t    pd3;
        }   f7;
        struct Format9 {
            uintptr_t   ia;
            uint16_t    ir[4];
        }   f9;
        struct FormatA {
            uint16_t    ir0;
            uint16_t    ssw;
            uint16_t    ipsc;
            uint16_t    ipsb;
            uintptr_t   dataCycleFaultAddress;
            uint16_t    ir1;
            uint16_t    ir2;
            uint32_t    dataOutputBuffer;
            uint16_t    ir3;
            uint16_t    ir4;
        }   fa;
        struct FormatB {
            uint16_t    ir0;
            uint16_t    ssw;
            uint16_t    ipsc;
            uint16_t    ipsb;
            uintptr_t   dataCycleFaultAddress;
            uint16_t    ir1;
            uint16_t    ir2;
            uint32_t    dataOutputBuffer;
            uint16_t    ir3;
            uint16_t    ir4;
            uint16_t    ir5;
            uint16_t    ir6;
            uintptr_t   stageBAddress;
            uint16_t    ir7;
            uint16_t    ir8;
            uint32_t    dataInputBuffer;
            uint16_t    ir9;
            uint16_t    ir10;
            uint16_t    ir11;
            uint16_t    version;
            uint16_t    ir[18];
        }   fb;
    }       u;
} excpt_frame_t;
#pragma pack()

#define excpt_frame_getsr(__ef) \
((__ef)->sr)

#define excpt_frame_isuser(__ef) \
(((__ef)->sr & CPU_SR_S) == 0)

#define excpt_frame_getpc(__ef) \
((__ef)->pc)

#define excpt_frame_setpc(__ef, __pc) \
(__ef)->pc = (uintptr_t)(__pc)

#define excpt_frame_getformat(__ef) \
((__ef)->fv >> 12)

#define excpt_frame_getvecoff(__ef) \
((__ef)->fv & 0x0fff)

#define excpt_frame_getvecnum(__ef) \
(excpt_frame_getvecoff(__ef) >> 2)


// FPU exception stack frame
// 68881/68882UM, p6-28
struct m68881_idle_frame {
    uint16_t    format;
    uint16_t    reserved;
    uint16_t    cmd_ccr;
    uint16_t    reserved2;
    uint32_t    ex_oper[3];
    uint32_t    oper_reg;
    uint32_t    biu_flags;
};

struct m68881_busy_frame {
    uint16_t    format;
    uint16_t    reserved;
    uint32_t    reg[45];
};


struct m68882_idle_frame {
    uint16_t    format;
    uint16_t    reserved;
    uint16_t    cmd_ccr;
    uint32_t    reg[8];
    uint32_t    ex_oper[3];
    uint32_t    oper_reg;
    uint32_t    biu_flags;
};

struct m68882_busy_frame {
    uint16_t    format;
    uint16_t    reserved;
    uint32_t    reg[53];
};


typedef struct fsave_frame {
    uint16_t    format;
    uint16_t    reserved;

    union m68881_2_extended_frame {
        struct m68881_idle_frame    idle881;
        struct m68881_busy_frame    busy881;
        struct m68882_idle_frame    idle882;
        struct m68882_busy_frame    busy882;
    }           u;
} fsave_frame_t;

#define fsave_frame_isnull(__sfp) \
(((__sfp)->format >> 8) == 0)

#define fsave_frame_getformat(__sfp) \
((__sfp)->format & 0xff)


// 68881/68882 frame formats
#define FSAVE_FORMAT_881_IDLE   0x18
#define FSAVE_FORMAT_881_BUSY   0xb4
#define FSAVE_FORMAT_882_IDLE   0x38
#define FSAVE_FORMAT_882_BUSY   0xd4


// BIU flags
#define BIU_OP_REG_24_31_VALID  (1 << 20)
#define BIU_OP_REG_16_23_VALID  (1 << 21)
#define BIU_OP_REG_8_15_VALID   (1 << 22)
#define BIU_OP_REG_0_7_VALID    (1 << 23)
#define BIU_OP_MEM_MV_PENDING   (1 << 26)
#define BIU_FP_EXCPT_PENDING    (1 << 27)
#define BIU_ACC_OP_REG_EXPECTED (1 << 28)
#define BIU_PENDING_INSTR_TYPE  (1 << 29)
#define BIU_INSTR_PENDING       (1 << 30)
#define BIU_PROTO_VIO_PENDING   (1 << 31)


extern unsigned int cpu68k_as_read_byte(void* p, int addr_space);
extern void cpu68k_as_write_byte(void* p, int addr_space, unsigned int val);

#endif /* _CPU_M68K_H */

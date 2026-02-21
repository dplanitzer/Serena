//
//  m68k/cpu.h
//  Kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _CPU_M68K_H
#define _CPU_M68K_H 1

#include <stdint.h>
#include <arch/cpu.h>
#include <arch/floattypes.h>

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
#define FPU_NULL_FSAVE_SIZE         4
#define FPU_68040_IDLE_FSAVE_SIZE   4
#define FPU_68881_IDLE_FSAVE_SIZE   28
#define FPU_68882_IDLE_FSAVE_SIZE   60
#define FPU_68040_UNIMP_FSAVE_SIZE  48
#define FPU_68040_BUSY_FSAVE_SIZE   96
#define FPU_68881_BUSY_FSAVE_SIZE   184
#define FPU_68882_BUSY_FSAVE_SIZE   216
#define FPU_MAX_FSAVE_SIZE          216     // Keep in sync with machine/hw/m68k/cpu.i
#define FPU_USER_STATE_SIZE         108     // Keep in sync with machine/hw/m68k/cpu.i


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


#define EXCPT_NUM_RESET_SSP  0
#define EXCPT_NUM_RESET_PC   1
#define EXCPT_NUM_BUS_ERR    2
#define EXCPT_NUM_ADR_ERR    3
#define EXCPT_NUM_ILLEGAL    4
#define EXCPT_NUM_ZERO_DIV   5
#define EXCPT_NUM_CHK        6
#define EXCPT_NUM_TRAPcc     7
#define EXCPT_NUM_PRIV_VIO   8
#define EXCPT_NUM_TRACE      9
#define EXCPT_NUM_LINE_A     10
#define EXCPT_NUM_LINE_F     11
#define EXCPT_NUM_EMU_INT    12
#define EXCPT_NUM_COPROC     13
#define EXCPT_NUM_FORMAT     14
#define EXCPT_NUM_UNINIT_IRQ 15
#define EXCPT_NUM_RESV_16    16
#define EXCPT_NUM_RESV_17    17
#define EXCPT_NUM_RESV_18    18
#define EXCPT_NUM_RESV_19    19
#define EXCPT_NUM_RESV_20    20
#define EXCPT_NUM_RESV_21    21
#define EXCPT_NUM_RESV_22    22
#define EXCPT_NUM_RESV_23    23
#define EXCPT_NNUM_SPUR_IRQ  24
#define EXCPT_NUM_IRQ_1      25
#define EXCPT_NUM_IRQ_2      26
#define EXCPT_NUM_IRQ_3      27
#define EXCPT_NUM_IRQ_4      28
#define EXCPT_NUM_IRQ_5      29
#define EXCPT_NUM_IRQ_6      30
#define EXCPT_NUM_IRQ_7      31
#define EXCPT_NUM_TRAP_0     32
#define EXCPT_NUM_TRAP_1     33
#define EXCPT_NUM_TRAP_2     34
#define EXCPT_NUM_TRAP_3     35
#define EXCPT_NUM_TRAP_4     36
#define EXCPT_NUM_TRAP_5     37
#define EXCPT_NUM_TRAP_6     38
#define EXCPT_NUM_TRAP_7     39
#define EXCPT_NUM_TRAP_8     40
#define EXCPT_NUM_TRAP_9     41
#define EXCPT_NUM_TRAP_10    42
#define EXCPT_NUM_TRAP_11    43
#define EXCPT_NUM_TRAP_12    44
#define EXCPT_NUM_TRAP_13    45
#define EXCPT_NUM_TRAP_14    46
#define EXCPT_NUM_TRAP_15    47
#define EXCPT_NUM_FPU_BRANCH_UO 48
#define EXCPT_NUM_FPU_INEXACT   49
#define EXCPT_NUM_FPU_DIV_ZERO  50
#define EXCPT_NUM_FPU_UNDERFLOW 51
#define EXCPT_NUM_FPU_OP_ERR    52
#define EXCPT_NUM_FPU_OVERFLOW  53
#define EXCPT_NUM_FPU_SNAN      54
#define EXCPT_NUM_FPU_UNIMPL_TY 55
#define EXCPT_NUM_MMU_CONFIG    56
#define EXCPT_NUM_PMMU_ILLEGAL  57
#define EXCPT_NUM_PMMU_ACCESS   58
#define EXCPT_NUM_RESV_59       59
#define EXCPT_NUM_UNIMPL_EA     60
#define EXCPT_NUM_UNIMPL_INST   61
#define EXCPT_NUM_RESV_62    62
#define EXCPT_NUM_RESV_63    63
#define EXCPT_NUM_USER_VEC   64

#define EXCPT_NUM_USER_VECS     192


// Format #0 CPU exception stack frame
// 68020UM, p6-27
#pragma pack(1)
typedef struct excpt_0_frame {
    uint16_t    sr;
    uintptr_t   pc;
    uint16_t    fv;
} excpt_0_frame_t;


// CPU exception stack frame
// 68020UM, p6-27
#pragma pack(1)
typedef struct excpt_frame {
    uint16_t    sr;
    uintptr_t   pc;
    uint16_t    fv;
    
    union {
        struct Format2 {
            // MC68020, MC68030, MC68040
            uintptr_t   addr;
        }   f2;
        struct Format3 {
            // MC68040
            uintptr_t   ea;
        }   f3;
        struct Format4_LineF {
            // MC68LC040, MC68EC040 and MC68060, MC68LC060, MC68EC060 for line F exceptions
            uintptr_t   ea;
            uintptr_t   pcFaultedInstr;
        }   f4_line_f;
        struct Format4_AccessError {
            // MC68060 for access (bus) error
            uintptr_t   faddr;
            uint32_t    fslw;
        }   f4_access_error;
        struct Format7 {
            // MC68040
            uintptr_t   ea;
            uint16_t    ssw;
            uint8_t     zero3;
            uint8_t     wb3s;
            uint8_t     zero2;
            uint8_t     wb2s;
            uint8_t     zero1;
            uint8_t     wb1s;
            uintptr_t   fa;
            uintptr_t   wb3a;
            uint32_t    wb3d;
            uintptr_t   wb2a;
            uint32_t    wb2d;
            uintptr_t   wb1a;
            uint32_t    wb1d_pd0;
            uint32_t    pd1;
            uint32_t    pd2;
            uint32_t    pd3;
        }   f7;
        struct Format9 {
            // MC68020, MC68030
            uintptr_t   ia;
            uint16_t    ir[4];
        }   f9;
        struct FormatA {
            // MC68020, MC68030
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
            // MC68020, MC68030
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


// MC68020, MC68030
// Exception frame type $A and $B
#define SSWAB_FC    (1 << 15)
#define SSWAB_FB    (1 << 14)
#define SSWAB_RC    (1 << 13)
#define SSWAB_RB    (1 << 12)
#define SSWAB_DF    (1 << 8)
#define SSWAB_RM    (1 << 7)
#define SSWAB_RW    (1 << 6)
#define SSWAB_SIZE_MASK     0x3
#define SSWAB_SIZE_SHIFT    4
#define SSWAB_FCx_MASK      0x7

#define sswab_get_size(__ssw) \
(((__ssw) >> SSWAB_SIZE_SHIFT) & SSWAB_SIZE_MASK)

#define sswab_get_fc(__ef) \
((__ssw) & SSWAB_FCx_MASK)

#define sswab_is_datafault(__ssw) \
(((__ssw) & SSWAB_DF) == SSWAB_DF)


// MC68040, MC68LC040, MC68EC040
// Exception frame type $7
#define SSW7_CP     (1 << 15)
#define SSW7_CU     (1 << 14)
#define SSW7_CT     (1 << 13)
#define SSW7_CM     (1 << 12)
#define SSW7_MA     (1 << 11)
#define SSW7_ATC    (1 << 10)
#define SSW7_LK     (1 << 9)
#define SSW7_RW     (1 << 8)
#define SSW7_X      (1 << 7)
#define SSW7_SIZE_MASK  0x3
#define SSW7_SIZE_SHIFT 5
#define SSW7_TT_MASK    0x3
#define SSW7_TT_SHIFT   3
#define SSW7_TM_MASK    0x7

#define ssw7_get_size(__ssw) \
(((__ssw) >> SSW7_SIZE_SHIFT) & SSW7_SIZE_MASK)

#define ssw7_get_tt(__ssw) \
(((__ssw) >> SSW7_TT_SHIFT) & SSW7_TT_MASK)

#define ssw7_get_tm(__ssw) \
((__ssw) & SSW7_TM_MASK)

// MC68040, p8-29 (248)
#define ssw7_is_read_access_error(__ssw) \
((((__ssw) & SSW7_RW) == SSW7_RW) && (ssw7_get_tt(__ssw) == 0) && ((ssw7_get_tm(__ssw) == 1) || (ssw7_get_tm(__ssw) == 5)))

#define ssw7_is_cache_push_phys_error(__ssw) \
((((__ssw) & SSW7_RW) == 0) && (ssw7_get_tt(__ssw) == 0) && (ssw7_get_tm(__ssw) == 0))

#define ssw7_is_write_phys_error(__ssw) \
((((__ssw) & SSW7_RW) == 0) && (ssw7_get_tt(__ssw) == 0) && ((ssw7_get_tm(__ssw) == 1) || (ssw7_get_tm(__ssw) == 5)))

#define ssw7_is_move16_write_phys_error(__ssw) \
((((__ssw) & SSW7_RW) == 0) && (ssw7_get_tt(__ssw) == 1))

#define excpt_frame7_is_page_fault(__efp) \
((((__efp)->u.f7.ssw & SSW7_RW) == 0) && (((__efp)->u.f7.wb1s & WBS7_V) == 0) && (((__efp)->u.f7.wb2s & WBS7_V) == WBS7_V))


// MC68040
// Exception frame type $7
#define WBS7_V      (1 << 7)
#define WBS7_SIZE_MASK  0x3
#define WBS7_SIZE_SHIFT 5
#define WBS7_TT_MASK    0x3
#define WBS7_TT_SHIFT   3
#define WBS7_TM_MASK    0x7

#define wbs7_is_valid(__wbs) \
(((__wbs) & WBS7_V) == WBS7_V)

#define wbs7_get_size(__wbs) \
(((__wbs) >> WBS7_SIZE_SHIFT) & WBS7_SIZE_MASK)

#define wbs7_get_tt(__ef) \
(((__wbs) >> WBS7_TT_SHIFT) & WBS7_TT_MASK)

#define wbs7_get_tm(__ef) \
((__wbs) & WBS7_TM_MASK)


// MC68060
// Exception frame type $4 [Access Error]
#define FSLW_MA     (1 << 27)
#define FSLW_LK     (1 << 25)
#define FSLW_RW_MASK    0x3
#define FSLW_RW_SHIFT   23
#define FSLW_SIZE_MASK  0x3
#define FSLW_SIZE_SHIFT 21
#define FSLW_TT_MASK    0x3
#define FSLW_TT_SHIFT   19
#define FSLW_TM_MASK    0x7
#define FSLW_TM_SHIFT   16
#define FSLW_IO     (1 << 15)
#define FSLW_PBE    (1 << 14)
#define FSLW_SBE    (1 << 13)
#define FSLW_PTA    (1 << 12)
#define FSLW_PTB    (1 << 11)
#define FSLW_IL     (1 << 10)
#define FSLW_PF     (1 << 9)
#define FSLW_SP     (1 << 8)
#define FSLW_WP     (1 << 7)
#define FSLW_TWE    (1 << 6)
#define FSLW_RE     (1 << 5)
#define FSLW_WE     (1 << 4)
#define FSLW_TTR    (1 << 3)
#define FSLW_BPE    (1 << 2)
#define FSLW_SEE    (1 << 0)

#define fslw_get_rw(__fslw) \
(((__fslw) >> FSLW_RW_SHIFT) & FSLW_RW_MASK)

#define fslw_get_size(__fslw) \
(((__fslw) >> FSLW_SIZE_SHIFT) & FSLW_SIZE_MASK)

#define fslw_get_tt(__fslw) \
(((__fslw) >> FSLW_TT_SHIFT) & FSLW_TT_MASK)

#define fslw_get_tm(__fslw) \
(((__fslw) >> FSLW_TM_SHIFT) & FSLW_TM_MASK)

#define fslw_is_push_buffer_error(__fslw) \
(((__fslw) & FSLW_BPE) == FSLW_BPE)

#define fslw_is_store_buffer_error(__fslw) \
(((__fslw) & FSLW_SBE) == FSLW_SBE)

#define fslw_is_misaligned_rmw(__fslw) \
((fslw_get_rw(__fslw) == 3) && (((__fslw) & FSLW_IO) == 0) && (((__fslw) & FSLW_MA) == 1))

#define fslw_is_self_overwriting_move(__fslw) \
(fslw_get_rw(__fslw) == 1)



// FPU exception stack frame
// 68881/68882UM, p6-28
// 68040UM, p9-39
// 68060UM, p6-35
struct m6888x_null_frame {
    uint8_t     version;
    uint8_t     format;
    uint16_t    reserved;
};

struct m68881_idle_frame {
    uint8_t     version;
    uint8_t     format;
    uint16_t    reserved;
    uint16_t    cmd_ccr;
    uint16_t    reserved2;
    uint32_t    ex_oper[3];
    uint32_t    oper_reg;
    uint32_t    biu_flags;
};

struct m68881_busy_frame {
    uint8_t     version;
    uint8_t     format;
    uint16_t    reserved;
    uint32_t    reg[45];
};


struct m68882_idle_frame {
    uint8_t     version;
    uint8_t     format;
    uint16_t    reserved;
    uint16_t    cmd_ccr;
    uint32_t    reg[8];
    uint32_t    ex_oper[3];
    uint32_t    oper_reg;
    uint32_t    biu_flags;
};

struct m68882_busy_frame {
    uint8_t     version;
    uint8_t     format;
    uint16_t    reserved;
    uint32_t    reg[53];
};


struct m68040_idle_frame {
    uint8_t     version;
    uint8_t     format;
    uint16_t    reserved;
};

struct m68040_busy_frame {
    uint8_t     version;
    uint8_t     format;
    uint16_t    reserved;
    uint8_t     reg[96];
};

struct m68040_unimp_frame {
    uint8_t     version;
    uint8_t     format;
    uint16_t    reserved;
    uint8_t     reg[48];
};


struct m68060_fsave_frame {
    uint16_t    operand_exp;
    uint8_t     format;
    uint8_t     vector;
    uint32_t    operand_upper;
    uint32_t    operand_lower;
};


typedef union fsave_frame {
    struct m6888x_null_frame    null;

    struct m68881_idle_frame    idle881;
    struct m68881_busy_frame    busy881;
    
    struct m68882_idle_frame    idle882;
    struct m68882_busy_frame    busy882;

    struct m68040_idle_frame    idle040;
    struct m68040_busy_frame    busy040;
    struct m68040_unimp_frame   unimp040;

    struct m68060_fsave_frame   frame060;
} fsave_frame_t;


extern bool cpu_is_null_fsave(const char* _Nonnull sfp);


// 68881/68882 frame versions
#define FSAVE_VERSION_68040     0x41


// 68881/68882 frame formats
#define FSAVE_FORMAT_881_IDLE       0x18
#define FSAVE_FORMAT_881_BUSY       0xb4
#define FSAVE_FORMAT_882_IDLE       0x38
#define FSAVE_FORMAT_882_BUSY       0xd4
#define FSAVE_FORMAT_68040_IDLE     0x00
#define FSAVE_FORMAT_68040_BUSY     0x60
#define FSAVE_FORMAT_68040_UNIMP    0x30
#define FSAVE_FORMAT_68060_IDLE     0x60
#define FSAVE_FORMAT_68060_EXCP     0xE0


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



// Describes the CPU register set that is saved on a context switch and when
// taking a CPU exception. Note that the exception frame 'ef' may have
// additional fields in the case of an exception.
// FP  state size: 324 bytes
// INT state size:  64 bytes
// CPU state size: 388 bytes
typedef struct cpu_savearea {
    uint32_t        fpiar;          // |
    uint32_t        fpsr;           // |
    uint32_t        fpcr;           // | only valid if the fsave[0] != 0 (and thus not a NULL fsave frame)
    float96_t       fp[8];          // |
    uint8_t         fsave[FPU_MAX_FSAVE_SIZE];

    uint32_t        usp;
    uint32_t        d[8];   
    uint32_t        a[7];

    excpt_0_frame_t ef;
} cpu_savearea_t;


// Describes the CPU register set that is saved when entering a system call.
typedef struct syscall_savearea {
    uint32_t        usp;
    uint32_t        d[8];   
    uint32_t        a[7];

    excpt_0_frame_t ef;
} syscall_savearea_t;


// Stores '__val' as the result of a system call invocation in the savearea of
// the virtual processor '__vp'.
#define syscall_setresult_int(__vp, __val) \
((__vp)->syscall_sa)->d[0] = (uint32_t)(__val)

// Stores '__ptr' as the result of a system call invocation in the savearea of
// the virtual processor '__vp'.
#define syscall_setresult_ptr(__vp, __ptr) \
((__vp)->syscall_sa)->d[0] = (uint32_t)(__ptr)


extern unsigned int cpu68k_as_read_byte(void* p, int addr_space);
extern void cpu68k_as_write_byte(void* p, int addr_space, unsigned int val);


#define M68060_PCR_ESS  (1 << 0)
#define M68060_PCR_DFP  (1 << 1)

extern void cpu060_set_pcr_bits(uint32_t bits);


// Grows the current user stack by 'pushing' 'nbytes' on it. Returns the new sp.
// Note that this function does NOT enforce stack alignment.
extern uintptr_t usp_grow(size_t nbytes);

// Shrinks the current user stack by 'popping off' 'nbytes'. Note that this
// function does NOT enforce stack alignment.
extern void usp_shrink(size_t nbytes);

// Grows the stack 'sp' by 'pushing' 'nbytes' on it. Returns the new sp. Note
// that this function does NOT enforce stack alignment.
extern uintptr_t sp_grow(uintptr_t sp, size_t nbytes);

// Shrinks the stack 'sp' by 'popping off' 'nbytes'. Note that this function
// does NOT enforce stack alignment.
extern void sp_shrink(uintptr_t sp, size_t nbytes);

#endif /* _CPU_M68K_H */

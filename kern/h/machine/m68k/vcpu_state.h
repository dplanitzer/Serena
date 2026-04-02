//
//  machine/m68k/vcpu_state.h
//  kpi
//
//  Created by Dietmar Planitzer on 12/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _MACHINE_M68K_VCPU_STATE_H
#define _MACHINE_M68K_VCPU_STATE_H 1

#include <_cmndef.h>
#include <machine/floattypes.h>

typedef struct vcpu_state_excpt {
    int             code;       // platform independent EXCPT_XXX code
    int             cpu_code;   // corresponding CPU specific code. Usually more detailed
    void* _Nonnull  sp;         // user stack pointer at the time of the exception
    void* _Nullable pc;         // program counter at the time of the exception (this is not always the instruction that caused the exception though)
    void* _Nullable fault_addr; // fault address
} vcpu_state_excpt_t;


typedef struct vcpu_state_m68k {
    unsigned long   d[8];
    unsigned long   a[8];
    unsigned long   pc;
    unsigned long   sr;     // CCR portion only (bits 0..7); rest is 0
} vcpu_state_m68k_t;

typedef struct vcpu_state_m68k_float {
    unsigned long   fpiar;
    unsigned long   fpsr;
    unsigned long   fpcr;
    float96_t       fp[8];
} vcpu_state_m68k_float_t;


#define _VCPU_STATE_IS_EXCPT    128

#define VCPU_STATE_M68K         1       /* vcpu_state_m68k_t */
#define VCPU_STATE_M68K_FLOAT   2       /* vcpu_state_m68k_float_t */

#define VCPU_STATE_EXCPT            (3 | _VCPU_STATE_IS_EXCPT)                      /* vcpu_state_excpt_t [read only] */
#define VCPU_STATE_EXCPT_M68K       (VCPU_STATE_M68K | _VCPU_STATE_IS_EXCPT)        /* vcpu_state_m68k_t */
#define VCPU_STATE_EXCPT_M68K_FLOAT (VCPU_STATE_M68K_FLOAT | _VCPU_STATE_IS_EXCPT)  /* vcpu_state_m68k_float_t */


#define _VCPU_IS_EXCPT_FLAVOR(__v) \
(((__v) & _VCPU_STATE_IS_EXCPT) == _VCPU_STATE_IS_EXCPT)

#define _VCPU_STRIP_EXCPT_FLAVOR(__v) \
((__v) & ~_VCPU_STATE_IS_EXCPT)

#endif /* _MACHINE_M68K_VCPU_STATE_H */

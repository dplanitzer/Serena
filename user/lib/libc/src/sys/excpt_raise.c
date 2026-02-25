//
//  excpt_raise.c
//  libc
//
//  Created by Dietmar Planitzer on 2/24/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <sys/exception.h>
#include <kpi/syscall.h>

extern int _excpt_raise(int cpu_code, void* _Nullable fault_addr);


int excpt_raise(int code, void* _Nullable fault_addr)
{
    int cpu_code;

    switch (code) {
#if defined (__M68K__)
        case EXCPT_ILLEGAL_INSTRUCTION:     cpu_code = 4; break;
        case EXCPT_PRIV_INSTRUCTION:        cpu_code = 8; break;
        case EXCPT_SOFT_INTERRUPT:          cpu_code = 47; break;
        case EXCPT_BOUNDS_EXCEEDED:         cpu_code = 6; break;
        case EXCPT_INT_DIVIDE_BY_ZERO:      cpu_code = 5; break;
        case EXCPT_INT_OVERFLOW:            cpu_code = 7; break;
        case EXCPT_BREAKPOINT:              cpu_code = 35; break;
        case EXCPT_SINGLE_STEP:             cpu_code = 9; break;
        case EXCPT_FLT_NAN:                 cpu_code = 54; break;
        case EXCPT_FLT_OPERAND:             cpu_code = 52; break;
        case EXCPT_FLT_OVERFLOW:            cpu_code = 53; break;
        case EXCPT_FLT_UNDERFLOW:           cpu_code = 51; break;
        case EXCPT_FLT_DIVIDE_BY_ZERO:      cpu_code = 50; break;
        case EXCPT_FLT_INEXACT:             cpu_code = 49; break;
        case EXCPT_INSTRUCTION_MISALIGNED:  cpu_code = 3; break;
        case EXCPT_DATA_MISALIGNED:         cpu_code = 2; break;
        case EXCPT_PAGE_ERROR:              cpu_code = 2; break;
        case EXCPT_ACCESS_VIOLATION:        cpu_code = 2; break;
#endif
        default:
            // leave error handling to _excpt_raise()
            cpu_code = 1;
            break;
    }
    return _excpt_raise(cpu_code, fault_addr);
}

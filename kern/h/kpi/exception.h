//
//  kpi/exception.h
//  kpi
//
//  Created by Dietmar Planitzer on 7/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_EXCEPTION_H
#define _KERN_EXCEPTION_H 1

#include <kpi/types.h>

typedef struct excpt_info {
    int             code;       // platform independent EXCPT_XXX code
    int             cpu_code;   // corresponding CPU specific code. Usually more detailed
    void* _Nonnull  sp;         // user stack pointer at the time of the exception
    void* _Nullable pc;         // program counter at the time of the exception (this is not always the instruction that caused the exception though)
    void* _Nullable fault_addr; // fault address
} excpt_info_t;


// Synchronously invoked when an exception happens. 'arg' is the argument provided
// at registration time and 'ei' is the exception information record.
//
// An exception handler should return EXCPT_CONTINUE_EXECUTION if it has fixed
// the problem that led to the exception and the vcpu should continue from the
// PC as stored in the exception information record. It should return
// EXCPT_ABORT_EXECUTION if the exception handler did not handle the exception
// and the process should exit and inform its parent process that the exit was
// due to an unhandled exception.
//
// An exception handler may read the full CPU state at the time when the
// exception was triggered by calling the vcpu_state() function with the desired
// VCPU_STATE_EXCPT_XXX flavor. The exception handler may update this state and
// write it back by calling the vcpu_setstate() function. The updated CPU state
// will be applied after the exception handler has return with
// EXCPT_CONTINUE_EXECUTION.
//
// Calling exit() or exec() from inside an exception handler clears the exception
// condition and marks the vcpu as "clean". For exit() this means that the
// parent process will be informed about a non-exceptional exit() and for exec()
// this means that the exec() call will replace the executable image and start
// executing the new image as if no exception would have happened.
typedef int (*excpt_func_t)(void* _Nullable arg, const excpt_info_t* _Nonnull ei);


typedef struct excpt_handler {
    excpt_func_t _Nonnull   func;
    void* _Nullable         arg;
} excpt_handler_t;


#define EXCPT_ILLEGAL_INSTRUCTION       1   /* illegal/undefined instruction */
#define EXCPT_PRIV_INSTRUCTION          2   /* privileged instruction */
#define EXCPT_SOFT_INTERRUPT            3   /* (software) interrupt, trap */
#define EXCPT_BOUNDS_EXCEEDED           4   /* check instruction detected a out-of-bounds condition (if supported by CPU) */
#define EXCPT_INT_DIVIDE_BY_ZERO        5   /* integer exceptions (division-by-zero, overflow, range check failed, etc) */
#define EXCPT_INT_OVERFLOW              6   /* integer overflow (if supported by CPU) */

#define EXCPT_BREAKPOINT                7   /* breakpoint instruction encountered (if supported by CPU) */
#define EXCPT_SINGLE_STEP               8   /* trace/single step instruction */

#define EXCPT_FLT_NAN                   9   /* fp instruction unexpectedly received a nan as input or a signalling nan was detected */
#define EXCPT_FLT_OPERAND               10  /* catch all for all other kinds of floating point exceptions */
#define EXCPT_FLT_OVERFLOW              11  /* fp overflow */
#define EXCPT_FLT_UNDERFLOW             12  /* fp underflow */
#define EXCPT_FLT_DIVIDE_BY_ZERO        13  /* fp division by zero */
#define EXCPT_FLT_INEXACT               14  /* fp result can not be exactly represented by a decimal fraction */

#define EXCPT_INSTRUCTION_MISALIGNED    15  /* instruction starts on a misaligned address */
#define EXCPT_DATA_MISALIGNED           16  /* instruction tries to r/w data that starts on a misaligned address */
#define EXCPT_PAGE_ERROR                17  /* attempted to access a page that does not exists or that couldn't be loaded into core */
#define EXCPT_ACCESS_VIOLATION          18  /* process lacks the rights to access this memory region */

#define EXCPT_FORCED_ABORT              19  /* execution was forcibly aborted by excpt_raise(EXCPT_FORCED_ABORT, NULL)/abort() */
#define EXCPT_USER                      20  /* user-defined exception triggered by excpt_raise(EXCPT_USER, ...) */
#define EXCPT_USER_2                    21  /* user-defined exception triggered by excpt_raise(EXCPT_USER_2, ...) */


//
// CPU specific exception code mapping information
//
// MC68000
// EXCPT_INSTRUCTION_MISALIGNED <- triggered when the CPU attempts to execute an instruction that start at an odd address
// EXCPT_DATA_MISALIGNED        <- triggered by a misaligned RMW or MOVE16 instruction on a 68040 or newer
// EXCPT_INT_OVERFLOW           <- triggered by a TRAPV instruction
// EXCPT_BREAKPOINT             <- triggered by a TRAP #3 instruction
//


// Exception handler return value
#define EXCPT_CONTINUE_EXECUTION    0   /* continue execution in the original execution context */
#define EXCPT_ABORT_EXECUTION       1   /* the exception was not handled and execution should be aborted and the process terminated */

#endif /* _KERN_EXCEPTION_H */

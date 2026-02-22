//
//  kpi/exception.h
//  kpi
//
//  Created by Dietmar Planitzer on 7/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_EXCEPTION_H
#define _KERN_EXCEPTION_H 1

#include <arch/cpu.h>
#include <kpi/types.h>

typedef struct excpt_info {
    int             code;       // EXCPT_XXX
    int             cpu_code;   // corresponding CPU code. Usually more detailed
    void* _Nullable addr;       // fault address
} excpt_info_t;


// Synchronously invoked when an exception happens. 'arg' is the argument provided
// at registration time. 'ei' is the exception information and 'mc' is the
// machine context (contents of CPU registers) at the time of the exception.
//
// An exception handler should return EXCPT_RES_HANDLED is it has fixed the
// problem that led to the exception and the vcpu should continue from the PC as
// stored in the machine context. It should return EXCPT_RES_UNHANDELED if the
// exception handler did not handled the exception and the process should exit
// and inform its parent process that the exit was due to an unhandled exception.
//
// Calling exit() or exec() from inside an exception handler clears the exception
// condition and marks the vcpu as "clean". For exit() this means that the
// parent process will be informed about a non-exceptional exit() and for exec()
// this means that the exec() call will replace the executable image and start
// executing the new image as if no exception would have happened.
typedef int (*excpt_func_t)(void* _Nullable arg, const excpt_info_t* _Nonnull ei, mcontext_t* _Nonnull mc);


typedef struct excpt_handler {
    excpt_func_t _Nonnull   func;
    void* _Nullable         arg;
} excpt_handler_t;


#define EXCPT_ILLEGAL       1   /* illegal/undefined instruction */
#define EXCPT_PRIVILEGED    2   /* privileged instruction */
#define EXCPT_TRAP          3   /* (software) interrupt, trap */
#define EXCPT_INT           4   /* integer exceptions (division-by-zero, overflow, range check failed, etc) */
#define EXCPT_FP            5   /* floating-point exceptions (division-by-zero, overflow, inexact, etc) */
#define EXCPT_TRACE         6   /* trace/single step instruction */

#define EXCPT_UNALIGNED     7   /* unaligned memory access */
#define EXCPT_BUS           8   /* bus error (accessed unmapped memory, misaligned r/w) */
#define EXCPT_ACCESS        9   /* memory access violation */

// Exception handler return value
#define EXCPT_RES_HANDLED   0   /* exception was handled; continue the original execution context */
#define EXCPT_RES_UNHANDLED -1  /* exception was NOT handled; execution exception default action (terminate process due to unhandled exception) */

#endif /* _KERN_EXCEPTION_H */

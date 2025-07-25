//
//  kpi/exception.h
//  libc
//
//  Created by Dietmar Planitzer on 7/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_EXCEPTION_H
#define _KERN_EXCEPTION_H 1

#include <kpi/types.h>

typedef struct excpt_info {
    int             code;       // EXCPT_XXX
    int             cpu_code;   // corresponding CPU code. Usually more detailed
    void* _Nullable addr;
} excpt_info_t;


typedef struct excpt_ctx {
    int dummy;
    //XXX machine context
    //XXX flag to control whether to continue or abort when handler returns
} excpt_ctx_t;


typedef void (*excpt_handler_t)(const excpt_info_t* _Nonnull ei, excpt_ctx_t* _Nonnull ctx);


#define EXCPT_ILLEGAL   1   /* illegal/undefined/privileged instruction */
#define EXCPT_TRAP      2   /* (software) interrupt, trap */
#define EXCPT_DIV_ZERO  3   /* integer division by zero */
#define EXCPT_FPE       4   /* floating-point exception */
#define EXCPT_BUS       5   /* bus error (accessed unmapped memory) */
#define EXCPT_SEGV      6   /* segmentation violation */


#define EXCPT_SCOPE_VCPU    0
#define EXCPT_SCOPE_PROC    1

#endif /* _KERN_EXCEPTION_H */

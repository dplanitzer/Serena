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


#define EXCPT_ILLEGAL   0   /* illegal/undefined instruction */
#define EXCPT_INTR      1   /* (software) interrupt, trap */
#define EXCPT_DIV_ZERO  2   /* integer division by zero */
#define EXCPT_FPE       3   /* floating-point exception */
#define EXCPT_BUS       5   /* bus error (accessed unmapped memory) */
#define EXCPT_SEGV      6   /* segmentation violation */

#endif /* _KERN_EXCEPTION_H */

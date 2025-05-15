//
//  kern/types.h
//  libc
//
//  Created by Dietmar Planitzer on 5/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_TYPES_H
#define _KERN_TYPES_H 1

#include <System/abi/_floattypes.h>
#include <System/abi/_off.h>
#include <System/abi/_size.h>
#include <System/abi/_ssize.h>
#include <System/_noreturn.h>
#include <System/_offsetof.h>
#include <System/_time.h>
#include <System/_varargs.h>
#include <System/Types.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/timespec.h>

__CPP_BEGIN

// Disk sector address based in cylinder, head, sector notation 
typedef struct chs {
    size_t  c;
    size_t  h;
    sno_t   s;
} chs_t;


typedef int32_t Quantums;             // Time unit of the scheduler clock which increments monotonically and once per quantum interrupt


// Function types 
typedef void (*VoidFunc_1)(void*);
typedef void (*VoidFunc_2)(void*, void*);

__CPP_END

#endif /* _KERN_TYPES_H */

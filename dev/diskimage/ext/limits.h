//
//  ext/limits.h
//  diskimage
//
//  Created by Dietmar Planitzer on 12/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _DI_LIMITS_H
#define _DI_LIMITS_H 1

#include <limits.h>

#if defined(_WIN32) || defined(_WIN64)

#define OFF_MIN     0x80000000ll
#define OFF_MAX     0x7fffffffll
#define OFF_WIDTH   32


#define SSIZE_MIN   0x8000000000000000l
#define SSIZE_MAX   0x7fffffffffffffffl
#define SSIZE_WIDTH 64

// SIZE_MAX is defined in <limits.h>
#define SIZE_WIDTH  64

#elif defined(__APPLE__)

// OFF_MIN, OFF_MAX, SSIZE_MIN, SSIZE_MAX are defined in <limits.h>
// SIZE_MAX is defined in <stdint.h>
#include <stdint.h>

#if defined(__LP64__)
#define OFF_WIDTH   64
#define SSIZE_WIDTH 64
#define SIZE_WIDTH  64
#else
#define OFF_WIDTH   32
#define SSIZE_WIDTH 32
#define SIZE_WIDTH  32
#endif /* __LP64__ */

#endif

#endif /* _DI_LIMITS_H */

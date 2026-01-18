//
//  ext/limits.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _EXT_LIMITS_H
#define _EXT_LIMITS_H 1

#include <limits.h>

#if defined(_WIN64)

#define OFF_MIN     0x80000000ll
#define OFF_MAX     0x7fffffffll
#define OFF_WIDTH   32


#define SSIZE_MIN   0x8000000000000000l
#define SSIZE_MAX   0x7fffffffffffffffl
#define SSIZE_WIDTH 64

// SIZE_MAX is defined in win64/limits.h
#define SIZE_WIDTH  64

#else

#define OFF_MIN  __OFF_MIN
#define OFF_MAX  __OFF_MAX
#define OFF_WIDTH __OFF_WIDTH


#define SSIZE_MIN  __SSIZE_MIN
#define SSIZE_MAX  __SSIZE_MAX
#define SSIZE_WIDTH __SSIZE_WIDTH


#define SIZE_MAX __SIZE_MAX
#define SIZE_WIDTH __SIZE_WIDTH

#endif

#endif /* _EXT_LIMITS_H */

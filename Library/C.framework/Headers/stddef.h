//
//  stddef.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDDEF_H
#define _STDDEF_H 1

#include <stdint.h>

#define __SIZE_WIDTH 64
#define __PTRDIFF_WIDTH __PTR_WIDTH


#if __SIZE_WIDTH == 32
typedef uint32_t size_t;
#elif __SIZE_WIDTH == 64
typedef uint64_t size_t;
#else
#error "unknown __SIZE_WIDTH"
#endif

#if __PTRDIFF_WIDTH == 32
typedef int32_t ptrdiff_t;
#elif __PTRDIFF_WIDTH == 64
typedef int64_t ptrdiff_t;
#else
#error "unknown __PTRDIFF_WIDTH"
#endif


#define NULL ((void*)0)

#define offset(type, member) __offsetof(type, member)

#endif /* _STDDEF_H */

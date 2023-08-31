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
#include <_nulldef.h>
#include <_sizedef.h>

#define __PTRDIFF_WIDTH __PTR_WIDTH


#if __PTRDIFF_WIDTH == 32
typedef int32_t ptrdiff_t;
#elif __PTRDIFF_WIDTH == 64
typedef int64_t ptrdiff_t;
#else
#error "unknown __PTRDIFF_WIDTH"
#endif


#define offset(type, member) __offsetof(type, member)

#endif /* _STDDEF_H */

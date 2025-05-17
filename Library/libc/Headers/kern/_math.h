//
//  kern/_math.h
//  libc
//
//  Created by Dietmar Planitzer on 9/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __SYS_MATH_H
#define __SYS_MATH_H 1

//#include <machine/abi/_dmdef.h>

#define __abs(x) (((x) < 0) ? -(x) : (x))
#ifndef __DISKIMAGE__
#define __min(x, y) (((x) < (y) ? (x) : (y)))
#define __max(x, y) (((x) > (y) ? (x) : (y)))
#endif
#define __clamped(v, lw, up) ((v) < (lw) ? (lw) : ((v) > (up) ? (up) : (v)))

#define __Ceil_PowerOf2(x, __align)   (((x) + ((__align)-1)) & ~((__align)-1))
#define __Floor_PowerOf2(x, __align) ((x) & ~((__align)-1))

#define __Ceil_Ptr_PowerOf2(x, __align)     (void*)(__Ceil_PowerOf2((__uintptr_t)(x), (__uintptr_t)(__align)))
#define __Floor_Ptr_PowerOf2(x, __align)     (void*)(__Floor_PowerOf2((__uintptr_t)(x), (__uintptr_t)(__align)))

#endif /* __SYS_MATH_H */

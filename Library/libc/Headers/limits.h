//
//  limits.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _LIMITS_H
#define _LIMITS_H 1

#include <machine/_dmdef.h>
#include <machine/_limits.h>
#include <stddef.h>

#define SIG_ATOMIC_WIDTH    __INT_WIDTH
#define SIG_ATOMIC_MIN      __INT_MIN
#define SIG_ATOMIC_MAX      __INT_MAX

#define MB_LEN_MAX 1

#define WINT_WIDTH  __UINT_WIDTH
#define WINT_MIN    0
#define WINT_MAX    UINT_MAX

#define WCHAR_WIDTH 16
#define WCHAR_MIN   0
#define WCHAR_MAX   USHRT_MAX

#endif /* _LIMITS_H */

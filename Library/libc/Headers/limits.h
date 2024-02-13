//
//  limits.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _LIMITS_H
#define _LIMITS_H 1

#include <System/abi/_dmdef.h>
#include <System/abi/_limits.h>
#include <stddef.h>

#define SIG_ATOMIC_MIN __INT_MIN
#define SIG_ATOMIC_MAX __INT_MAX

#define MB_LEN_MAX 2

#define WINT_MIN 0
#define WINT_MAX UINT_MAX

#define WCHAR_MIN 0
#define WCHAR_MAX USHRT_MAX

#endif /* _LIMITS_H */

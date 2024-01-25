//
//  limits.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _LIMITS_H
#define _LIMITS_H 1

#include <_cmndef.h>
#include <_dmdef.h>
#include <stddef.h>

__CPP_BEGIN

#define CHAR_BIT 8
#define CHAR_MIN 0x80
#define CHAR_MAX 0x7f

#define SCHAR_MIN 0x80
#define SCHAR_MAX 0x7f

#define SHRT_MIN 0x8000
#define SHRT_MAX 0x7fff

#define INT_MIN __INT_MIN
#define INT_MAX __INT_MAX

#define LONG_MIN 0x80000000l
#define LONG_MAX 0x7fffffffl

#define LLONG_MIN 0x8000000000000000ll
#define LLONG_MAX 0x7fffffffffffffffll

#define UCHAR_MAX 255
#define USHRT_MAX 65535
#define UINT_MAX __UINT_MAX
#define ULONG_MAX 4294967295ul
#define ULLONG_MAX 18446744073709551615ull

#define PTRDIFF_MIN __PTRDIFF_MIN
#define PTRDIFF_MAX __PTRDIFF_MAX

#define SIZE_MAX __SIZE_MAX

#define SIG_ATOMIC_MIN __INT_MIN
#define SIG_ATOMIC_MAX __INT_MAX

#define MB_LEN_MAX 2

#define WINT_MIN 0
#define WINT_MAX UINT_MAX

#define WCHAR_MIN 0
#define WCHAR_MAX USHRT_MAX

__CPP_END

#endif /* _LIMITS_H */

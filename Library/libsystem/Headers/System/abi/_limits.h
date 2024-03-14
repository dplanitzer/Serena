//
//  _limits.h
//  libsystem
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __ABI_LIMITS_H
#define __ABI_LIMITS_H 1

#include <System/abi/_dmdef.h>

#ifdef __SYSTEM_SHIM__
#include <limits.h>
#else

#define BOOL_WIDTH  8

#define CHAR_WIDTH 8
#define CHAR_BIT 8
#define CHAR_MIN 0x80
#define CHAR_MAX 0x7f

#define SCHAR_WIDTH 8
#define SCHAR_MIN 0x80
#define SCHAR_MAX 0x7f

#define SHRT_WIDTH 16
#define SHRT_MIN 0x8000
#define SHRT_MAX 0x7fff

#define INT_WIDTH __INT_WIDTH
#define INT_MIN __INT_MIN
#define INT_MAX __INT_MAX

#define LONG_WIDTH __LONG_WIDTH
#define LONG_MIN __LONG_MIN
#define LONG_MAX __LONG_MAX

#define LLONG_WIDTH __LLONG_WIDTH
#define LLONG_MIN __LLONG_MIN
#define LLONG_MAX __LLONG_MAX

#define UCHAR_WIDTH 8
#define UCHAR_MAX 255

#define USHRT_WIDTH 16
#define USHRT_MAX 65535

#define UINT_WIDTH __UINT_WIDTH
#define UINT_MAX __UINT_MAX

#define ULONG_WIDTH __ULONG_WIDTH
#define ULONG_MAX __ULONG_MAX

#define ULLONG_WIDTH __ULLONG_WIDTH
#define ULLONG_MAX __ULLONG_MAX

#define PTRDIFF_WIDTH __PTRDIFF_WIDTH
#define PTRDIFF_MIN __PTRDIFF_MIN
#define PTRDIFF_MAX __PTRDIFF_MAX

#define SIZE_WIDTH __SIZE_WIDTH
#define SIZE_MAX __SIZE_MAX

#define BITINT_MAXWIDTH __ULLONG_WIDTH

#endif /* __SYSTEM_SHIM__ */
#endif /* __ABI_LIMITS_H */

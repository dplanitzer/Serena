//
//  limits.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _LIMITS_H
#define _LIMITS_H 1

#define CHAR_BIT 8
#define CHAR_MIN -128
#define CHAR_MAX 127

#define SCHAR_MIN -128
#define SCHAR_MAX 127

#define SHRT_MIN -32768
#define SHRT_MAX 32767

#define INT_MIN -2147483648
#define INT_MAX 2147483647

#define LONG_MIN -2147483648l
#define LONG_MAX 2147483647l

#define LLONG_MIN -9223372036854775808ll
#define LLONG_MAX 9223372036854775807ll

#define UCHAR_MAX 255
#define USHRT_MAX 65535
#define UINT_MAX 4294967295u
#define ULONG_MAX 4294967295ul
#define ULLONG_MAX 18446744073709551615ull

#define PTRDIFF_MIN -2147483648l
#define PTRDIFF_MAX 2147483647l

#define SIZE_MAX ULLONG_MAX

#define SIG_ATOMIC_MIN INT_MIN
#define SIG_ATOMIC_MAX INT_MAX

#define MB_LEN_MAX 2

#define WINT_MIN 0
#define WINT_MAX UINT_MAX

#define WCHAR_MIN 0
#define WCHAR_MAX USHRT_MAX

#endif /* _LIMITS_H */

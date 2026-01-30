//
//  ext/stdlib.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _EXT_STDLIB_H
#define _EXT_STDLIB_H 1

#if ___STDC_HOSTED__ != 1
#error "not supported in freestanding mode"
#endif

#include <stdlib.h>

__CPP_BEGIN

// Valid values for 'radix' are: 2, 8, 10, 16
extern char *itoa(int val, char *buf, int radix);
extern char *ltoa(long val, char *buf, int radix);
extern char *lltoa(long long val, char *buf, int radix);

extern char *utoa(unsigned int val, char *buf, int radix);
extern char *ultoa(unsigned long val, char *buf, int radix);
extern char *ulltoa(unsigned long long val, char *buf, int radix);

__CPP_END

#endif /* _EXT_STDLIB_H */

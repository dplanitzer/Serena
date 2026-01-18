//
//  _absdiv.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 12/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef __ABSDIV_H
#define __ABSDIV_H 1

#include <_cmndef.h>

__CPP_BEGIN

extern int abs(int n);
extern long labs(long n);
extern long long llabs(long long n);


typedef struct div_t { int quot; int rem; } div_t;
typedef struct ldiv_t { long quot; long rem; } ldiv_t;
typedef struct lldiv_t { long long quot; long long rem; } lldiv_t;

extern div_t div(int x, int y);
extern ldiv_t ldiv(long x, long y);
extern lldiv_t lldiv(long long x, long long y);


typedef struct udiv_t { unsigned int quot; unsigned int rem; } udiv_t;
typedef struct uldiv_t { unsigned long quot; unsigned long rem; } uldiv_t;
typedef struct ulldiv_t { unsigned long long quot; unsigned long long rem; } ulldiv_t;

extern udiv_t udiv(unsigned int x, unsigned int y);
extern uldiv_t uldiv(unsigned long x, unsigned long y);
extern ulldiv_t ulldiv(unsigned long long x, unsigned long long y);

__CPP_END

#endif /* __ABSDIV_H */

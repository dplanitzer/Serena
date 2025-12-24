//
//  assert.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#if ___STDC_HOSTED__ != 1
#error "not supported in freestanding mode"
#endif

#include <_cmndef.h>

__CPP_BEGIN

#if NDEBUG

#define assert(ignored)    ((void)0)

#else

extern void _Assert(const char* _Nonnull _Restrict filename, int lineNum, const char* _Nonnull _Restrict funcName, const char* _Nonnull _Restrict expr);

#ifndef assert
#define assert(cond)   if ((cond) == 0) { _Assert(__FILE__, __LINE__, __func__, #cond); }
#endif

#endif /* NDEBUG */

__CPP_END

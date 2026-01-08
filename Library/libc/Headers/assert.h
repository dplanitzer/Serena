//
//  assert.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <_cmndef.h>

__CPP_BEGIN

#if NDEBUG

#define assert(ignored)    ((void)0)

#else

#ifndef assert

#if __ASSERT_LOD == 0
extern void _Assert_failed0(void);
#define assert(cond)   if ((cond) == 0) { _Assert_failed0(); }
#elif __ASSERT_LOD == 3
extern void _Assert_failed3(const char* _Nonnull _Restrict filename, int lineno, const char* _Nonnull _Restrict funcname, const char* _Nonnull _Restrict expr);
#define assert(cond)   if ((cond) == 0) { _Assert_failed2(__FILE__, __LINE__, __func__, #cond); }
#elif __ASSERT_LOD == 2
extern void _Assert_failed2(int lineno, const char* _Nonnull _Restrict funcname, const char* _Nonnull _Restrict expr);
#define assert(cond)   if ((cond) == 0) { _Assert_failed1(__LINE__, __func__, #cond); }
#else
extern void _Assert_failed1(int lineno, const char* _Nonnull _Restrict funcname);
#define assert(cond)   if ((cond) == 0) { _Assert_failed1(__LINE__, __func__); }
#endif

#endif

#endif /* NDEBUG */

__CPP_END

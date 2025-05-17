//
//  assert.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <_cmndef.h>
#ifndef NDEBUG
#include <stdnoreturn.h>
#endif /* NDEBUG */

__CPP_BEGIN

#if NDEBUG

#define assert(ignored)    ((void)0)

#else

extern _Noreturn _Assert(const char* _Nonnull pFilename, int lineNum, const char* _Nonnull pFuncName, const char* _Nonnull expr);

#ifndef assert
#define assert(cond)   if ((cond) == 0) { _Assert(__FILE__, __LINE__, __func__, #cond); }
#endif

#endif /* NDEBUG */

__CPP_END

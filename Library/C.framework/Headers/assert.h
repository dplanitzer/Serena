//
//  assert.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _ASSERT_H
#define _ASSERT_H 1

#include <_cmndef.h>
#include <_nulldef.h>
#include <stdnoreturn.h>

__CPP_BEGIN

#if NDEBUG
#define assert(cond)    ((void)0)
#else
extern _Noreturn _Assert(const char* _Nonnull pFilename, int lineNum, const char* _Nonnull pFuncName, const char* _Nonnull expr);

#define assert(cond)   if ((cond) == 0) { _Assert(__FILE__, __LINE__, __func__, #cond); }
#endif

__CPP_END

#endif /* _ASSERT_H */

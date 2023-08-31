//
//  assert.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _ASSERT_H
#define _ASSERT_H 1

#include <stdnoreturn.h>

extern _Noreturn _Assert(const char* pFilename, int lineNum, const char* pFuncName, const char* expr);

#if NDEBUG
#define assert(cond)    ((void)0)
#else
#define assert(cond)   if ((cond) == 0) { _Assert(__FILE__, __LINE__, __func__, #cond); }
#endif

#endif /* _ASSERT_H */

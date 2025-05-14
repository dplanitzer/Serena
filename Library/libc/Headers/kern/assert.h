//
//  kern/assert.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/17/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_ASSERT_H
#define _KERN_ASSERT_H 1

#include <System/_cmndef.h>
#include <System/_noreturn.h>

struct ExceptionStackFrame;


extern _Noreturn fatal(const char* _Nonnull format, ...);
extern _Noreturn fatalError(const char* _Nonnull filename, int line, int err);
extern _Noreturn fatalAbort(const char* _Nonnull filename, int line);
extern _Noreturn fatalAssert(const char* _Nonnull filename, int line);
extern _Noreturn _fatalException(const struct ExceptionStackFrame* _Nonnull pFrame);


#define abort() fatalAbort(__func__, __LINE__)


#if DEBUG
#define assert(cond)   if ((cond) == 0) { fatalAssert(__func__, __LINE__); }
#else
#define assert(cond)
#endif

#endif /* _KERN_ASSERT_H */

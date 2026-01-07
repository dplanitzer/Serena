//
//  kern/assert.h
//  libc
//
//  Created by Dietmar Planitzer on 8/17/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_ASSERT_H
#define _KERN_ASSERT_H 1

#include <_cmndef.h>
#include <stdnoreturn.h>

#if DEBUG
extern _Noreturn fatalAssert(const char* _Nonnull filename, int line);
#define assert(cond)   if ((cond) == 0) { fatalAssert(__func__, __LINE__); }
#else
#define assert(cond)
#endif

#endif /* _KERN_ASSERT_H */

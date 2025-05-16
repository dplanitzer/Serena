//
//  sys/errno.h
//  libc
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_ERRNO_H
#define _SYS_ERRNO_H 1

#include <assert.h>
#include <errno.h>
#include <kern/_try.h>
#include <stdnoreturn.h>

// This is 'errno.h' plus try() macros

// Halt the machine if the function 'f' does not return EOK. Use this instead of
// 'try' if you are calling a failable function but based on the design of the
// code the function you call should never fail in actual reality.
extern _Noreturn _Abort(const char* _Nonnull filename, int lineNum, const char* _Nonnull funcName);
#define _Try_bang(f)         { const errno_t _err_ = (f);  if (_err_ != 0) { _Abort(__FILE__, __LINE__, __func__); }}

#ifndef __cplusplus
// Make the official names available in C. We don't in C++ because some of these
// names would clash with the C++ exception handling related keywords. C++ code
// should use the underscored names defined above.

#define try_bang(f) _Try_bang(f)
#endif  /* __cplusplus */

#endif /* _SYS_ERRNO_H */

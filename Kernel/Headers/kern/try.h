//
//  kern/try.h
//  libc
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_TRY_H
#define _KERN_TRY_H 1

#include <kern/assert.h>
#include <ext/_try.h>

// Halt the machine if the function 'f' does not return EOK. Use this instead of
// 'try' if you are calling a failable function but based on the design of the
// code the function you call should never fail in actual reality.
#define _Try_bang(f)         { const errno_t _err_ = (f);  if (_err_ != EOK) { fatalError(__func__, __LINE__, (int) _err_); }}

#ifndef __cplusplus
// Make the official names available in C. We don't in C++ because some of these
// names would clash with the C++ exception handling related keywords. C++ code
// should use the underscored names defined above.

#define try_bang(f) _Try_bang(f)
#endif  /* __cplusplus */

#endif /* _KERN_TRY_H */

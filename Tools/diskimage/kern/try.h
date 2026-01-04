//
//  kern/try.h
//  diskimage
//
//  Created by Dietmar Planitzer on 5/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_TRY_H
#define _KERN_TRY_H 1

#include <errno.h>
#include <kern/assert.h>

#define _EOK    0
#define EOK     _EOK

#ifndef EACCESS
#define EACCESS EACCES
#endif
#ifndef ENOTIOCTLCMD
#define ENOTIOCTLCMD EINVAL
#endif

#include <../../Library/libc/Headers/ext/_try.h>

// Halt the machine if the function 'f' does not return EOK. Use this instead of
// 'try' if you are calling a failable function but based on the design of the
// code the function you call should never fail in actual reality.
#define _Try_bang(f)         { const errno_t _err_ = (f);  if (_err_ != 0) { printf("%s:%d:%s\n", __FILE__, __LINE__, __func__); abort(); }}

#ifndef __cplusplus
// Make the official names available in C. We don't in C++ because some of these
// names would clash with the C++ exception handling related keywords. C++ code
// should use the underscored names defined above.

#define try_bang(f) _Try_bang(f)
#endif  /* __cplusplus */

#endif /* _KERN_TRY_H */

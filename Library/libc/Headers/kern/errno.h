//
//  kern/errno.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_ERRNO_H
#define _KERN_ERRNO_H 1

#include <System/Error.h>
#include <kern/assert.h>

// Halt the machine if the function 'f' does not return EOK. Use this instead of
// 'try' if you are calling a failable function but based on the design of the
// code the function you call should never fail in actual reality.
#define try_bang(f)         { const errno_t _err_ = (f);  if (_err_ != EOK) { fatalError(__func__, __LINE__, (int) _err_); }}

#endif /* _KERN_ERRNO_H */

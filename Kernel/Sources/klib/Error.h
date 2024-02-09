//
//  Error.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Error_h
#define Error_h

#include <klib/Assert.h>
#include <abi/_errno.h>
#include <abi/_errtrycatch.h>


// Error code definitions
typedef __errno_t errno_t;
#define EOK __EOK

// Halt the machine if the function 'f' does not return EOK. Use this instead of
// 'try' if you are calling a failable function but based on the design of the
// code the function you call should never fail in actual reality.
#define try_bang(f)         { const errno_t _err_ = (f);  if (_err_ != EOK) { fatalError(__func__, __LINE__, (int) _err_); }}

#endif /* Error_h */

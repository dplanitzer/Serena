//
//  Error.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Error_h
#define Error_h

#include <klib/Types.h>
#include <klib/Assert.h>


// Error code definitions (keep in sync with lowmem.i)
typedef Int ErrorCode;
#define EOK         0
#define ENOMEM      1
#define ENOMEDIUM   2
#define EDISKCHANGE 3
#define ETIMEDOUT   4
#define ENODEV      5
#define EPARAM      6
#define ERANGE      7
#define EINTR       8
#define EAGAIN      9
#define EPIPE       10
#define EBUSY       11
#define ENOSYS      12
#define EINVAL      13
#define EIO         14
#define EPERM       15
#define EDEADLK     16
#define EDOM        17
#define ENOEXEC     18
#define E2BIG       19
#define ENOENT      20
#define ENOTBLK     21


// Macros to detect errors and to jump to the 'failed:' label if an error is detected.

// Declares an error variable 'err' which is assigned to by the try_xxx macros
// and available at the 'catch' label
#define decl_try_err()      ErrorCode err = EOK

// Go to the 'catch' label if 'f' does not return EOK. The error returned by 'f'
// is assigned to 'err'. Call this instead of 'try_bang' if you are calling a
// failable function and it is by design expected that the function can actually
// fail at runtime and there is a way to recover from the failure. Most of the
// time 'recovering' from a failure will simply mean to abort the system call
// and to return to user space. The application will then need to figure out
// how to proceed.
#define try(f)              if ((err = (f)) != EOK) goto catch

// Go to the 'catch' label if 'f' returns a NULL pointer. The pointer is stored
// in the variable 'p'. 'e' is the error that should be assigned to 'err'
#define try_null(p, f, e)   if ((p = (f)) == NULL) { err = e; goto catch; }

// Halt the machine if the function 'f' does not return EOK. Use this instead of
// 'try' if you are calling a failable function but based on the design of the
// code the function you call should never fail in actual reality.
#define try_bang(f)         { const ErrorCode _err_ = (f);  if (_err_ != EOK) { fatalError(__func__, __LINE__, (Int) _err_); }}

// Set 'err' to the given error and go to the 'catch' label if the given pointer
// is null. Otherwise fall through to the next statement.
#define throw_ifnull(p, e)  if ((p) == NULL) { err = e; goto catch; }

// Set 'err' to the given error and go to the 'catch' label.
#define throw(e)            err = e; goto catch;

#endif /* Error_h */

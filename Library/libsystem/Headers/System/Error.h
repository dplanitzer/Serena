//
//  Error.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_ERROR_H
#define _SYS_ERROR_H

#include <System/_cmndef.h>
#include <System/_errno.h>

__CPP_BEGIN

typedef _Errno_t errno_t;
#define EOK _EOK


// Macros to detect errors and to jump to the 'failed:' label if an error is detected.

// Declares an error variable 'err' which is assigned to by the try_xxx macros
// and available at the 'catch' label
#define _Decl_try_err()      errno_t err = _EOK

// Go to the 'catch' label if 'f' does not return EOK. The error returned by 'f'
// is assigned to 'err'. Call this instead of 'try_bang' if you are calling a
// failable function and it is by design expected that the function can actually
// fail at runtime and there is a way to recover from the failure. Most of the
// time 'recovering' from a failure will simply mean to abort the system call
// and to return to user space. The application will then need to figure out
// how to proceed.
#define _Try(f)              if ((err = (f)) != _EOK) goto catch

// Go to the 'catch' label if 'f' returns a NULL pointer. The pointer is stored
// in the variable 'p'. 'e' is the error that should be assigned to 'err'
#define _Try_null(p, f, e)   if ((p = (f)) == NULL) { err = e; goto catch; }

// Halt the machine if the function 'f' does not return EOK. Use this instead of
// 'try' if you are calling a failable function but based on the design of the
// code the function you call should never fail in actual reality.
//XXX not yet
//#define try_bang(f)         { const errno_t _err_ = (f);  if (_err_ != EOK) { fatalError(__func__, __LINE__, (int) _err_); }}
//#define try_bang(f)         { const errno_t _err_ = (f);  if (_err_ != 0) { __abort(__FILE__, __LINE__, __func__); }}

// Set 'err' to the given error and go to the 'catch' label if the given pointer
// is null. Otherwise fall through to the next statement.
#define _Throw_ifnull(p, e)  if ((p) == NULL) { err = e; goto catch; }

// Set 'err' to the given error and go to the 'catch' label if the given error
// is not EOK. Otherwise fall through to the next statement.
#define _Throw_iferr(e)  if ((e) != EOK) { err = e; goto catch; }

// Set 'err' to the given error and go to the 'catch' label.
#define _Throw(e)            {err = e; goto catch;}


#ifndef __cplusplus

// Make the official names available in C. We don't in C++ because some of these
// names would clash with the C++ exception handling related keywords. C++ code
// should use the underscored names defined above.

#define decl_try_err        _Decl_try_err
#define try(f)              _Try(f)
#define try_null(p, f, e)   _Try_null(p, f, e)
#define throw_ifnull(p, e)  _Throw_ifnull(p, e)
#define throw_iferr(e)      _Throw_iferr(e)
#define throw(e)            _Throw(e)

#endif  /* __cplusplus */

__CPP_END

#endif /* _SYS_ERROR_H */

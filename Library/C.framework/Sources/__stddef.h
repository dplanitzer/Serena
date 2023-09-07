//
//  __stddef.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef ___STDDEF_H
#define ___STDDEF_H 1

#include <_cmndef.h>
#include <_math.h>
#include <stdalign.h>
#include <stddef.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdnoreturn.h>

extern _Noreturn _Abort(const char* _Nonnull pFilename, int lineNum, const char* _Nonnull pFuncName);

typedef __errno_t errno_t;

// Macros to detect errors and to jump to the 'failed:' label if an error is detected.

// Declares an error variable 'err' which is assigned to by the try_xxx macros
// and available at the 'catch' label
#define decl_try_err()      errno_t err = 0

// Go to the 'catch' label if 'f' does not return EOK. The error returned by 'f'
// is assigned to 'err'. Call this instead of 'try_bang' if you are calling a
// failable function and it is by design expected that the function can actually
// fail at runtime and there is a way to recover from the failure. Most of the
// time 'recovering' from a failure will simply mean to abort the system call
// and to return to user space. The application will then need to figure out
// how to proceed.
#define try(f)              if ((err = (f)) != 0) goto catch

// Go to the 'catch' label if 'f' returns a NULL pointer. The pointer is stored
// in the variable 'p'. 'e' is the error that should be assigned to 'err'
#define try_null(p, f, e)   if ((p = (f)) == NULL) { err = e; goto catch; }

// Halt the machine if the function 'f' does not return EOK. Use this instead of
// 'try' if you are calling a failable function but based on the design of the
// code the function you call should never fail in actual reality.
#define try_bang(f)         { const errno_t _err_ = (f);  if (_err_ != 0) { _Abort(__FILE__, __LINE__, __func__); }}

// Set 'err' to the given error and go to the 'catch' label if the given pointer
// is null. Otherwise fall through to the next statement.
#define throw_ifnull(p, e)  if ((p) == NULL) { err = e; goto catch; }

// Set 'err' to the given error and go to the 'catch' label.
#define throw(e)            err = e; goto catch;


extern errno_t __Int64_DivMod(int64_t dividend, int64_t divisor, int64_t *quotient, int64_t *remainder);

extern void __stdlibc_init(void);
extern void __malloc_init(void);

#endif /* ___STDDEF_H */

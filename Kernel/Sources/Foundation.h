//
//  Foundation.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Foundation_h
#define Foundation_h

#include <Runtime.h>


#define SIZE_GB(x)  ((Int)(x) * 1024 * 1024 * 1024)
#define SIZE_MB(x)  ((Int)(x) * 1024 * 1024)
#define SIZE_KB(x)  ((Int)(x) * 1024)


// Macros to detect errors and to jump to the 'failed:' label if an error is detected.
#define FailErr(x)      if ((x) != EOK) goto failed
#define FailFalse(p)    if (!(p)) goto failed
#define FailNULL(p)     if ((p) == NULL) goto failed
#define FailZero(p)     if ((p) == 0) goto failed


// Error code definitions
typedef Int ErrorCode;
#define EOK         0
#define ENOMEM      1
#define ENODATA     2
#define ENOTCDF     3
#define ENOBOOT     4
#define ENODRIVE    5
#define EDISKCHANGE 6
#define ETIMEOUT    7
#define ENODEVICE   8
#define EPARAM      9
#define ERANGE      10
#define EINTR       11
#define EAGAIN      12
#define EPIPE       13
#define EBUSY       14


// Int64
extern const Character* _Nonnull Int64_ToString(Int64 val, Int base, Int fieldWidth, Character paddingChar, Character* _Nonnull pString, Int maxLength);


// UInt64
extern const Character* _Nonnull UInt64_ToString(UInt64 val, Int base, Int fieldWidth, Character paddingChar, Character* _Nonnull pString, Int maxLength);


// Printing
extern void print(const Character* _Nonnull format, ...);


// Asserts
extern _Noreturn fatalError(const Character* _Nonnull filename, int line);

#define abort() fatalError(__func__, __LINE__)

#if DEBUG
#define assert(cond)   if ((cond) == 0) { abort(); }
#else
#define assert(cond)
#endif

extern _Noreturn mem_non_recoverable_error(void);

#endif /* Foundation_h */

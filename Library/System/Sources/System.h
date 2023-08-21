//
//  System.h
//  Apollo
//
//  Created by Dietmar Planitzer on 7/9/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef System_h
#define System_h

#include <Runtime.h>


// Error code definitions
typedef Int ErrorCode;
#define EOK         0
#define ENOMEM      1
#define ENODATA     2
#define ENOTCDF     3
#define ENOBOOT     4
#define ENODRIVE    5
#define EDISKCHANGE 6
#define ETIMEDOUT   7
#define ENODEV      8
#define EPARAM      9
#define ERANGE      10
#define EINTR       11
#define EAGAIN      12
#define EPIPE       13
#define EBUSY       14
#define ENOSYS      15
#define EINVAL      16
#define EIO         17


extern ErrorCode write(Character* _Nonnull pString);
extern ErrorCode dispatch_async(void* _Nonnull pClosure);
extern ErrorCode sleep(Int seconds, Int nanoseconds);
extern ErrorCode alloc_address_space(Int nbytes, Byte* _Nullable * _Nonnull pOutMemPtr);
extern _Noreturn exit(Int status);

#endif /* System_h */

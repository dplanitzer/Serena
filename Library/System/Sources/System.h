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
#define ETIMEOUT    7
#define ENODEVICE   8
#define EPARAM      9
#define ERANGE      10
#define EINTR       11
#define EAGAIN      12
#define EPIPE       13
#define EBUSY       14


extern void print(Character* _Nonnull pString);
extern void dispatchAsync(void* _Nonnull pClosure);
extern void sleep(Int seconds, Int nanoseconds);

#endif /* System_h */

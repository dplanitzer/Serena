//
//  DispatchQueue.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_DISPATCH_QUEUE_H
#define _SYS_DISPATCH_QUEUE_H 1

#include <System/_cmndef.h>
#include <System/_nulldef.h>
#include <System/Error.h>
#include <System/Types.h>

__CPP_BEGIN

typedef void (*Dispatch_Closure)(void* _Nullable arg);

#define kDispatchQueue_Main 0


#if !defined(__KERNEL__)

// Schedules the given closure for asynchronous execution on the given dispatch
// queue. The 'pContext' argument will be passed to the callback.
extern errno_t DispatchQueue_DispatchAsync(int od, Dispatch_Closure _Nonnull pClosure, void* _Nullable pContext);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_DISPATCH_QUEUE_H */

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
#include <System/types.h>

__CPP_BEGIN

typedef void (*Dispatch_Closure)(void* _Nullable arg);


#if !defined(__KERNEL__)

extern errno_t DispatchQueue_Async(Dispatch_Closure _Nonnull pClosure);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_DISPATCH_QUEUE_H */

//
//  DispatchQueue.c
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/DispatchQueue.h>
#include <System/_syscall.h>



errno_t DispatchQueue_DispatchAsync(int od, Dispatch_Closure _Nonnull pClosure)
{
    return _syscall(SC_dispatch_async, od, pClosure);
}

//
//  DispatchQueue.c
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <apollo/DispatchQueue.h>
#include <apollo/syscall.h>



errno_t DispatchQueue_Async(Dispatch_Closure _Nonnull pClosure)
{
    return _syscall(SC_dispatch_async, pClosure);
}

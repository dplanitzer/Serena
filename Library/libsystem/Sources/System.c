//
//  System.c
//  libsystem
//
//  Created by Dietmar Planitzer on 4/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/Types.h>
#include <System/Process.h>
#include <System/_syscall.h>
#include <System/_varargs.h>

extern void __UrtInit(ProcessArguments* _Nonnull argsp);


static bool __gIsSystemLibInitialized;

void System_Init(ProcessArguments* _Nonnull argsp)
{
    if (__gIsSystemLibInitialized) {
        return;
    }
    
    __UrtInit(argsp);
    __gIsSystemLibInitialized = true;
}

errno_t System_ConInit(void)
{
    return (errno_t)_syscall(SC_coninit);
}

//
//  System.c
//  libsystem
//
//  Created by Dietmar Planitzer on 4/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/Types.h>
#include <System/Process.h>

extern void __UrtInit(ProcessArguments* _Nonnull argsp);
extern void __AllocatorInit(void);


static bool __gIsSystemLibInitialized;

void System_Init(ProcessArguments* _Nonnull argsp)
{
    if (__gIsSystemLibInitialized) {
        return;
    }
    
    __UrtInit(argsp);
    __AllocatorInit();
    __gIsSystemLibInitialized = true;
}

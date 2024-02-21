//
//  Pipe.c
//  libsystem
//
//  Created by Dietmar Planitzer on 2/20/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/Pipe.h>
#include <System/_syscall.h>
#include <System/_varargs.h>


errno_t Pipe_Create(int* _Nonnull rioc, int* _Nonnull wioc)
{
    return (errno_t)_syscall(SC_mkpipe, rioc, wioc);
}

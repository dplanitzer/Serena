//
//  Disk.c
//  libsystem
//
//  Created by Dietmar Planitzer on 12/25/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/Disk.h>
#include <System/_syscall.h>
#include <System/_varargs.h>


void os_sync(void)
{
    _syscall(SC_sync);
}

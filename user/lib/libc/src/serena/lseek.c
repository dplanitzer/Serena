//
//  lseek.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <unistd.h>
#include <kpi/syscall.h>


off_t lseek(int fd, off_t offset, int whence)
{
    off_t newpos;
    
    if (_syscall(SC_lseek, fd, offset, &newpos, whence) == 0) {
        return newpos;
    }
    else {
        return -1ll;
    }
}

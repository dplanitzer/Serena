//
//  fd_seek.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/fd.h>


off_t fd_seek(int fd, off_t offset, int whence)
{
    off_t newpos;
    
    if (_syscall(SC_fd_seek, fd, offset, &newpos, whence) == 0) {
        return newpos;
    }
    else {
        return -1ll;
    }
}

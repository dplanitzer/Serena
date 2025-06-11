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
    off_t oldpos;
    
    if (_syscall(SC_lseek, fd, offset, &oldpos, whence) == 0) {
        return oldpos;
    }
    else {
        return -1ll;
    }
}

//
//  system.c
//  libc
//
//  Created by Dietmar Planitzer on 2/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/wait.h>
#include <System/Process.h>


int system(const char *string)
{
    decl_try_err();
    pid_t shPid;
    pstatus_t pts;
    spawn_opts_t opts = {0};
    const char* argv[4];

    if (string == NULL) {
        errno = EINVAL;
        return -1;
    }

    argv[0] = "shell";
    argv[1] = "-c";
    argv[2] = string;
    argv[3] = NULL;


    err = os_spawn("/System/Commands/shell", argv, &opts, &shPid);
    if (err != EOK) {
        errno = err;
        return -1;
    }

    if (waitpid(shPid, &pts) != 0) {
        return -1;
    }

    return pts.status;
}

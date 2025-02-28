//
//  system.c
//  libc
//
//  Created by Dietmar Planitzer on 2/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <__stddef.h>
#include <System/System.h>


int system(const char *string)
{
    decl_try_err();
    pid_t shPid;
    ProcessTerminationStatus pts;
    SpawnOptions opts = {0};
    const char* argv[4];

    if (string == NULL) {
        return -1;
    }

    argv[0] = "shell";
    argv[1] = "-c";
    argv[2] = string;
    argv[3] = NULL;


    err = Process_Spawn("/System/Commands/shell", argv, &opts, &shPid);
    if (err != EOK) {
        errno = err;
        return -1;
    }

    err = Process_WaitForTerminationOfChild(shPid, &pts);
    if (err != EOK) {
        errno = err;
        return -1;
    }

    return (err == EOK) ? 0 : pts.status;
}

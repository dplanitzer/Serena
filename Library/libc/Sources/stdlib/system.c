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
#include <sys/spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>


static const char* __shellPath = "/System/Commands/shell";


static int __has_shell(void)
{
    struct stat st;

    if (stat(__shellPath, &st) == 0) {
        if (S_ISREG(st.st_mode) && (st.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) != 0) {
            return -1;
        }
    }
    
    return 0;
}

static int __system(const char *string)
{
    pid_t sh_pid;
    int sh_stat;
    spawn_opts_t opts = {0};
    const char* argv[4];
    
    argv[0] = __shellPath;
    argv[1] = "-c";
    argv[2] = string;
    argv[3] = NULL;

    if (os_spawn(__shellPath, argv, &opts, &sh_pid) != 0) {
        return -1;
    }

    if (waitpid(sh_pid, &sh_stat, 0) < 0) {
        return -1;
    }

    return WEXITSTATUS(sh_stat);
}

int system(const char *string)
{
    if (string) {
        return __system(string);
    }
    else {
        return __has_shell();
    }
}

//
//  system.c
//  libc
//
//  Created by Dietmar Planitzer on 2/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/vcpu.h>


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
    vcpuid_t vp_id = vcpu_id(vcpu_self());
    int r = 0;
    spawn_opts_t opts = {0};
    const char* argv[4];
    struct proc_status ps;

    argv[0] = __shellPath;
    argv[1] = "-c";
    argv[2] = string;
    argv[3] = NULL;

    // Enable SIGCHLD reception
    sigroute(SIG_ROUTE_ADD, SIGCHLD, SIG_SCOPE_VCPU, vp_id);

    if (os_spawn(__shellPath, argv, &opts, &sh_pid) != 0) {
        r = -1;
        goto out;
    }

    if (proc_join(JOIN_PROC, sh_pid, &ps) < 0) {
        r = -1;
        goto out;
    }

out:
    sigroute(SIG_ROUTE_DEL, SIGCHLD, SIG_SCOPE_VCPU, vp_id);

    return (r == 0) ? (ps.reason == JREASON_EXIT) ? ps.u.status : EXIT_FAILURE : -1;
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

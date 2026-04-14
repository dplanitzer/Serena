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
#include <serena/file.h>
#include <serena/process.h>
#include <serena/signal.h>
#include <serena/vcpu.h>


static const char* __shellPath = "/System/Commands/shell";


static int __has_shell(void)
{
    fs_attr_t attr;

    if (fs_attr(NULL, __shellPath, &attr) == 0) {
        if ((attr.file_type == FS_FTYPE_REG) && (attr.permissions & (FS_USR_X|FS_GRP_X|FS_OTH_X)) != 0) {
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
    proc_spawn_t opts = {0};
    const char* argv[4];
    proc_status_t ps;

    argv[0] = __shellPath;
    argv[1] = "-c";
    argv[2] = string;
    argv[3] = NULL;

    // Enable SIG_CHILD reception
    sig_route(SIG_ROUTE_ADD, SIG_CHILD, SIG_SCOPE_VCPU, vp_id);

    if (proc_spawn(__shellPath, argv, &opts, &sh_pid) != 0) {
        r = -1;
        goto out;
    }

    if (proc_status(PROC_STOF_PID, sh_pid, 0, &ps) < 0) {
        r = -1;
        goto out;
    }

out:
    sig_route(SIG_ROUTE_DEL, SIG_CHILD, SIG_SCOPE_VCPU, vp_id);

    return (r == 0) ? (ps.reason == PROC_STATUS_EXITED) ? ps.u.status : EXIT_FAILURE : -1;
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

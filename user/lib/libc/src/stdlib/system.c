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
#include <serena/spawn.h>
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
    vcpuid_t vp_id = vcpu_id(vcpu_self());
    int r = 0;
    proc_spawnattr_t sa;
    proc_spawnres_t sres;
    const char* argv[4];
    proc_waitres_t ps;

    proc_spawnattr_init(&sa);

    argv[0] = __shellPath;
    argv[1] = "-c";
    argv[2] = string;
    argv[3] = NULL;

    // Enable SIG_CHILD reception
    sig_route(SIG_ROUTE_ADD, SIG_CHILD, SIG_SCOPE_VCPU, vp_id);


    r = proc_spawn(__shellPath, argv, NULL, &sa, NULL, &sres);
    if (r == 0) {
        r = proc_waitstate(WAIT_FOR_TERMINATED, WAIT_PID, sres.pid, 0, &ps);
    }

    
    sig_route(SIG_ROUTE_DEL, SIG_CHILD, SIG_SCOPE_VCPU, vp_id);
    proc_spawnattr_destroy(&sa);

    return (r == 0) ? (ps.reason == WAIT_REASON_EXITED) ? ps.u.status : EXIT_FAILURE : -1;
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

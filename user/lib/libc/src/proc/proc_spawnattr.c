//
//  proc_spawnattr.c
//  libc
//
//  Created by Dietmar Planitzer on 4/26/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <serena/spawn.h>
#include <string.h>

int proc_spawnattr_init(proc_spawnattr_t* _Nonnull attr)
{
    memset(attr, 0, sizeof(struct proc_spawnattr));
    attr->version = sizeof(struct proc_spawnattr);
    attr->type = PROC_SPAWN_GROUP_MEMBER;
    attr->pgrp = 0;
    
    return 0;
}

int proc_spawnattr_destroy(proc_spawnattr_t* _Nullable attr)
{
    if (attr) {
        memset(attr, 0, sizeof(struct proc_spawnattr));
    }

    return 0;
}

int proc_spawnattr_type(const proc_spawnattr_t* _Nonnull attr)
{
    return attr->type;
}

int proc_spawnattr_settype(proc_spawnattr_t* _Nonnull attr, int type)
{
    if (type < PROC_SPAWN_GROUP_MEMBER || type > PROC_SPAWN_SESSION_LEADER) {
        errno = EINVAL;
        return -1;
    }

    attr->type = type;
    attr->pgrp = 0;
}

pid_t proc_spawnattr_pgrp(const proc_spawnattr_t* _Nonnull attr)
{
    return attr->pgrp;
}

int proc_spawnattr_setpgrp(proc_spawnattr_t * _Nonnull attr, pid_t pgrp)
{
    if (pgrp < 0) {
        errno = EINVAL;
        return -1;
    }

    attr->pgrp = pgrp;
}

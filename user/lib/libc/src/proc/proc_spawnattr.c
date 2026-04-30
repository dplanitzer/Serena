//
//  proc_spawnattr.c
//  libc
//
//  Created by Dietmar Planitzer on 4/26/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <kpi/process.h>
#include <serena/spawn.h>

int proc_spawnattr_init(proc_spawnattr_t* _Nonnull attr)
{
    attr->version = _PROC_SPAWNATTR_VERSION;
    attr->type = PROC_SPAWN_GROUP_MEMBER;
    attr->pgrp = 0;
    attr->umask = 0;
    attr->uid = 0;
    attr->gid = 0;
    attr->flags = 0;
    attr->quantum_boost = 0;
    attr->nice = 0;

    return 0;
}

int proc_spawnattr_destroy(proc_spawnattr_t* _Nullable attr)
{
    if (attr) {
        attr->version = 0;
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


fs_perms_t proc_spawnattr_umask(const proc_spawnattr_t* _Nonnull attr)
{
    return attr->umask;
}

int proc_spawnattr_setumask(proc_spawnattr_t* _Nonnull attr, fs_perms_t umask)
{
    attr->umask = umask & 0777;
    attr->flags |= _PROC_SPAFL_UMASK;
    return 0;
}


uid_t proc_spawnattr_uid(const proc_spawnattr_t* _Nonnull attr)
{
    return attr->uid;
}

int proc_spawnattr_setuid(proc_spawnattr_t* _Nonnull attr, uid_t uid)
{
    attr->uid = uid;
    attr->flags |= _PROC_SPAFL_UID;
    return 0;
}


gid_t proc_spawnattr_gid(const proc_spawnattr_t* _Nonnull attr)
{
    return attr->gid;
}

int proc_spawnattr_setgid(proc_spawnattr_t* _Nonnull attr, gid_t gid)
{
    attr->gid = gid;
    attr->flags |= _PROC_SPAFL_GID;
    return 0;
}


bool proc_spawnattr_suspended(const proc_spawnattr_t* _Nonnull attr)
{
    return (attr->flags & _PROC_SPAFL_SUSPENDED) == _PROC_SPAFL_SUSPENDED ? true : false;
}

void proc_spawnattr_setsuspended(proc_spawnattr_t* _Nonnull attr, bool flag)
{
    if (flag) {
        attr->flags |= _PROC_SPAFL_SUSPENDED;
    }
    else {
        attr->flags &= ~_PROC_SPAFL_SUSPENDED;
    }
}


int proc_spawnattr_schedparam(const proc_spawnattr_t* _Nonnull attr, int type, int* _Nonnull param)
{
    switch (type) {
        case SCHED_QUANTUM_BOOST:
            *param = attr->quantum_boost;
            return 0;

        case SCHED_NICE:
            *param = attr->nice;
            return 0;

        default:
            errno = EINVAL;
            return -1;
    }
}

int proc_spawnattr_setschedparam(proc_spawnattr_t* _Nonnull attr, int type, const int* _Nonnull param)
{
    if (*param < 0) {
        errno = ERANGE;
        return -1;
    }
    
    switch (type) {
        case SCHED_QUANTUM_BOOST:
            attr->quantum_boost = *param;
            return 0;

        case SCHED_NICE:
            attr->nice = *param;
            return 0;

        default:
            errno = EINVAL;
            return -1;
    }
}

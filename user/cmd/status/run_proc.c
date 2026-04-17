//
//  run_proc.c
//  status
//
//  Created by Dietmar Planitzer on 4/15/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "run_proc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <serena/host.h>
#include <serena/process.h>

static deque_t  g_run_procs;
static size_t   g_run_proc_count;
static char     g_path_buf[PATH_MAX];
const char*     g_run_proc_state_name[5] = {
    "running",      // RUN_PROC_RUNNING
    "sleeping",     // RUN_PROC_SLEEPING
    "stopped",      // RUN_PROC_STOPPED
    "running",      // RUN_PROC_TERMINATING
    "zombie",       // RUN_PROC_TERMINATED
};

static run_procs_info_t g_info;


const char* _Nonnull run_proc_state_name(int state)
{
    if (state >= 0 && state < 5) {
        return g_run_proc_state_name[state];
    }
    else {
        return "??";
    }
}

static run_proc_t* _Nullable run_proc_for_pid(pid_t pid)
{
    deque_for_each(&g_run_procs, struct run_proc, it,
        if (it->pid == pid) {
            return it;
        }
    );

    return NULL;
}

run_proc_t* _Nullable run_proc_for_index(size_t idx)
{
    if (idx < g_run_proc_count) {
        deque_for_each(&g_run_procs, struct run_proc, it,
            if (idx == 0) {
                return it;
            }

            idx--;
        );
    }

    return NULL;
}

size_t run_proc_count(void)
{
    return g_run_proc_count;
}

static run_proc_t* _Nullable run_proc_acquire(pid_t pid)
{
    run_proc_t* rp = run_proc_for_pid(pid);
    
    if (rp == NULL) {
        rp = calloc(1, sizeof(struct run_proc));
        rp->pid = pid;
        deque_add_last(&g_run_procs, &rp->pid_node);
        g_run_proc_count++;
    }

    return rp;
}

static void run_proc_relinquish(run_proc_t* _Nullable rp)
{
    if (rp) {
        deque_remove(&g_run_procs, &rp->pid_node);
        free(rp);
        g_run_proc_count--;
    }
}


static int state_from_basic_info(const proc_basic_info_t* _Nonnull ip)
{
    switch (ip->run_state) {
        case PROC_STATE_RESUMED:
            return (ip->vcpu_waiting_count == ip->vcpu_count) ? RUN_PROC_SLEEPING : RUN_PROC_RUNNING;

        case PROC_STATE_SUSPENDED:
            return RUN_PROC_STOPPED;

        case PROC_STATE_TERMINATING:
            return RUN_PROC_TERMINATING;

        case PROC_STATE_TERMINATED:
            return RUN_PROC_TERMINATED;

        default:
            return RUN_PROC_RUNNING;
    }
}

static void run_proc_sample(pid_t pid)
{
    proc_basic_info_t basic;
    proc_ids_info_t ids;
    proc_user_info_t creds;

    errno = 0;

    proc_info(pid, PROC_INFO_BASIC, &basic);
    proc_info(pid, PROC_INFO_IDS, &ids);
    proc_info(pid, PROC_INFO_USER, &creds);
    proc_property(pid, PROC_PROP_PATH, g_path_buf, sizeof(g_path_buf));

    if (errno != 0) {
        return;
    }

    const char* lastPathComponent = strrchr(g_path_buf, '/');
    const char* pnam = (lastPathComponent) ? lastPathComponent + 1 : g_path_buf;

    run_proc_t* rp = run_proc_acquire(pid);
    if (rp == NULL) {
        return;
    }

    rp->pgrp = ids.group_id;
    rp->ppid = ids.parent_id;
    rp->sid = ids.session_id;
    rp->uid = creds.uid;
    rp->vcpu_count = basic.vcpu_count;
    rp->vm_size = basic.vm_size;
    rp->state = state_from_basic_info(&basic);
    strncpy(rp->name, pnam, MAX_PROC_NAME_LEN);


    g_info.proc_count++;
    if (rp->state == RUN_PROC_RUNNING) {
        g_info.run_proc_count++;
    }
    else if (rp->state == RUN_PROC_SLEEPING || rp->state == RUN_PROC_STOPPED) {
        g_info.slp_proc_count++;
    }
    g_info.vcpu_count += basic.vcpu_count;
}

void run_procs_sample(void)
{
#define PID_BUF_SIZE  33
    static pid_t g_pid_buf[PID_BUF_SIZE];

    if (host_processes(g_pid_buf, PID_BUF_SIZE) < 0) {
        return;
    }

    memset(&g_info, 0, sizeof(struct run_procs_info));

    size_t i = 0;
    while (g_pid_buf[i] > 0) {
        run_proc_sample(g_pid_buf[i]);
        i++;
    }


    host_basic_info_t host_basic;
    if (host_info(HOST_INFO_BASIC, &host_basic) == 0) {
        g_info.cpu_count = host_basic.physical_cpu_count;
        g_info.phys_mem_size = host_basic.phys_mem_size;
    }
}

const run_procs_info_t* _Nonnull run_procs_info()
{
    return &g_info;
}

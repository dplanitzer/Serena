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
#include <serena/clock.h>
#include <serena/host.h>
#include <serena/process.h>

static deque_t  g_run_procs;    //XXX change to hashtable keyed on pid
static size_t   g_run_proc_count;
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
    static proc_basic_info_t basic;
    static proc_ids_info_t ids;
    static proc_user_info_t creds;
    static proc_times_info_t times;

    run_proc_t* rp = run_proc_acquire(pid);
    if (rp == NULL) {
        return;
    }


    errno = 0;
    proc_info(pid, PROC_INFO_BASIC, &basic);
    proc_info(pid, PROC_INFO_IDS, &ids);
    proc_info(pid, PROC_INFO_USER, &creds);
    proc_info(pid, PROC_INFO_TIMES, &times);

    if (errno != 0) {
        run_proc_relinquish(rp);
        return;
    }


    rp->pgrp = ids.group_id;
    rp->ppid = ids.parent_id;
    rp->sid = ids.session_id;
    rp->uid = creds.uid;
    rp->vcpu_count = basic.vcpu_count;
    nanotime_sub(&g_info.current_time, &times.creation_time, &rp->run_time);
    rp->vm_size = basic.vm_size;
    rp->state = state_from_basic_info(&basic);
    rp->flags.alive = 1;
    if (proc_property(pid, PROC_PROP_NAME, rp->name, sizeof(rp->name)) != 0) {
        strcpy(rp->name, "??");
    }


    g_info.proc_count++;
    if (rp->state == RUN_PROC_RUNNING) {
        g_info.run_proc_count++;
    }
    else if (rp->state == RUN_PROC_SLEEPING || rp->state == RUN_PROC_STOPPED) {
        g_info.slp_proc_count++;
    }
    g_info.vcpu_count += basic.vcpu_count;
}

static const pid_t* _Nonnull get_all_proc_pids(void)
{
#define INIT_PID_BUF_SIZE   128
    static pid_t* pid_buf = NULL;
    static size_t pid_buf_size = 0;

    if (pid_buf == NULL) {
        pid_buf = malloc(INIT_PID_BUF_SIZE * sizeof(pid_t));
        if (pid_buf == NULL) {
            return NULL;
        }

        pid_buf_size = INIT_PID_BUF_SIZE;
    }

    while (host_processes(pid_buf, pid_buf_size) == 1) {
        size_t new_pid_buf_size = pid_buf_size * 2;
        pid_t* new_pid_buf = realloc(pid_buf, new_pid_buf_size * sizeof(pid_t));

        if (new_pid_buf == NULL) {
            return NULL;
        }

        pid_buf = new_pid_buf;
        pid_buf_size = new_pid_buf_size;
    }

    return pid_buf;
}

void run_procs_sample(void)
{
    const pid_t* pids = get_all_proc_pids();

    if (pids == NULL) {
        return;
    }


    // Update the global info
    memset(&g_info, 0, sizeof(struct run_procs_info));

    host_basic_info_t host_basic;
    if (host_info(HOST_INFO_BASIC, &host_basic) == 0) {
        g_info.cpu_count = host_basic.physical_cpu_count;
        g_info.phys_mem_size = host_basic.phys_mem_size;
    }


    clock_time(CLOCK_MONOTONIC, &g_info.current_time);


    // Create new run procs for processes we haven't seen before, update processes
    // we've seen before and then relinquish run procs that no longer exist.
    deque_for_each(&g_run_procs, struct run_proc, it,
        it->flags.alive = 0;
    );

    size_t i = 0;
    while (pids[i] > 0) {
        run_proc_sample(pids[i++]);
    }

    deque_for_each(&g_run_procs, struct run_proc, it,
        if (!it->flags.alive) {
            run_proc_relinquish(it);
        }
    );
}

const run_procs_info_t* _Nonnull run_procs_info()
{
    return &g_info;
}

size_t run_procs_count(void)
{
    return g_run_proc_count;
}

void run_procs_get_all(run_proc_t** buf, size_t bufSize)
{
    size_t i = 0;

    deque_for_each(&g_run_procs, struct run_proc, it,
        if (i >= bufSize) {
            break;
        }

        buf[i++] = it;
    );
}

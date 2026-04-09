//
//  status.c
//  cmds
//
//  Created by Dietmar Planitzer on 5/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <ext/stdlib.h>
#include <serena/host.h>
#include <serena/process.h>

enum {
    kState_Running = 0,
    kState_Sleeping,
    kState_Stopped,
    kState_Exiting,
    kState_Zombie
};

static char path_buf[PATH_MAX];
static char num_buf[__LONG_MAX_BASE_10_DIGITS + 1];
static const char* state_name[5] = {
    "running",      // kState_Running
    "sleeping",     // kState_Sleeping
    "stopped",      // kState_Stopped
    "running",      // kState_Exiting
    "zombie",       // kState_Zombie
};


CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("status")
);


static void fmt_mem_size(size_t msize, char* _Nonnull buf)
{
#define N_UNITS 4
    static const char* postfix[N_UNITS] = { "", "K", "M", "G" };
    int pi = 0;

    for (;;) {
        if (msize < 1024 || pi == N_UNITS) {
            ultoa((unsigned long)msize, buf, 10);
            strcat(buf, postfix[pi]);
            break;
        }

        msize >>= 10;
        pi++;
    }
}

static int state_from_basic_info(const proc_basic_info_t* _Nonnull ip)
{
    switch (ip->run_state) {
        case PROC_STATE_RESUMED:
            return (ip->vcpu_waiting_count == ip->vcpu_count) ? kState_Sleeping : kState_Running;

        case PROC_STATE_SUSPENDED:
            return kState_Stopped;

        case PROC_STATE_TERMINATING:
            return kState_Exiting;

        case PROC_STATE_TERMINATED:
            return kState_Zombie;

        default:
            return kState_Running;
    }
}

static void show_proc(pid_t pid)
{
    proc_basic_info_t basic;
    proc_ids_info_t ids;
    proc_user_info_t creds;

    errno = 0;

    proc_info(pid, PROC_INFO_BASIC, &basic);
    proc_info(pid, PROC_INFO_IDS, &ids);
    proc_info(pid, PROC_INFO_USER, &creds);
    proc_property(pid, PROC_PROP_PATH, path_buf, sizeof(path_buf));

    if (errno != 0) {
        return;
    }


    const char* lastPathComponent = strrchr(path_buf, '/');
    const char* pnam = (lastPathComponent) ? lastPathComponent + 1 : path_buf;

    if (pid > 1) {
        fmt_mem_size(basic.vm_size, num_buf);
    }
    else {
        strcpy(num_buf, "-");
    }

    printf("%d  %s  %zu  %s  %s  %d  %d  %d  %d\n", pid, pnam, basic.vcpu_count, num_buf, state_name[state_from_basic_info(&basic)], creds.uid, ids.session_id, ids.group_id, ids.parent_id);
}

static int show_procs(void)
{
#define _TMP_MAX_COUNT  33
    static pid_t g_pid_buf[_TMP_MAX_COUNT];

    if (host_processes(g_pid_buf, _TMP_MAX_COUNT) < 0) {
        return -1;
    }
    
    printf("PID  Command  #VP  Memory  State  UID  SID  PGRP  PPID\n");

    size_t idx = 0;
    while (g_pid_buf[idx] > 0) {
        show_proc(g_pid_buf[idx]);
        idx++;
    }

    return (errno == 0) ? 0 : -1;
}


int main(int argc, char* argv[])
{
    clap_parse(0, params, argc, argv);
    
    if (show_procs() == 0) {
        return EXIT_SUCCESS;
    }
    else {
        clap_error(argv[0], "%s", strerror(errno));
        return EXIT_FAILURE;
    }
}

//
//  status.c
//  cmds
//
//  Created by Dietmar Planitzer on 5/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <ext/stdlib.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/stat.h>


static char path_buf[PATH_MAX];
static char num_buf[__LONG_MAX_BASE_10_DIGITS + 1];
static const char* state_name[5] = {
    "running",      // PROC_STATE_RUNNING
    "sleeping",     // PROC_STATE_SLEEPING
    "stopped",      // PROC_STATE_STOPPED
    "running",      // PROC_STATE_EXITING
    "zombie",       // PROC_STATE_ZOMBIE
};


CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("status")
);


static int open_proc(const char* _Nonnull pidStr)
{
    sprintf(path_buf, "/proc/%s", pidStr);

    const int fd = open(path_buf, O_RDONLY);
    return (fd >= 0) ? fd : -1;
}

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

static void show_proc(const char* _Nonnull pidStr)
{
    proc_info_t info;
    const int fd = open_proc(pidStr);

    if (fd != -1) {
        ioctl(fd, kProcCommand_GetInfo, &info);
        ioctl(fd, kProcCommand_GetName, path_buf, sizeof(path_buf));

        if (errno == 0) {
            const char* lastPathComponent = strrchr(path_buf, '/');
            const char* pnam = (lastPathComponent) ? lastPathComponent + 1 : path_buf;

            if (info.pid > 1) {
                fmt_mem_size(info.virt_size, num_buf);
            }
            else {
                strcpy(num_buf, "-");
            }

            printf("%d  %s  %zu  %s  %s  %d  %d  %d  %d\n", info.pid, pnam, info.vcpu_count, num_buf, state_name[info.state], info.uid, info.sid, info.pgrp, info.ppid);
        }

        close(fd);
    }
}

static int show_procs(void)
{
    DIR* dir;

    dir = opendir("/proc");
    if (dir) {
        printf("PID  Command  #VP  Memory  State  UID  SID  PGRP  PPID\n");

        for (;;) {
            struct dirent* dep = readdir(dir);
            
            if (dep == NULL) {
                break;
            }

            if (dep->name[0] != '.') {
                show_proc(dep->name);
            }
        }
        closedir(dir);
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

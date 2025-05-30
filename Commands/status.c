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
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/stat.h>


static char path_buf[PATH_MAX];

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

            printf("%d  %s  %d  %zu\n", info.pid, pnam, info.ppid, info.virt_size);
        }

        close(fd);
    }
}

static int show_procs(void)
{
    DIR* dir;

    dir = opendir("/proc");
    if (dir) {
        printf("PID  Command  PPID  Memory\n");

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

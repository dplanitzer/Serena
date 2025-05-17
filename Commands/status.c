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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/limits.h>
#include <sys/proc.h>
#include <sys/stat.h>


CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("status")
);


static errno_t show_proc(const char* _Nonnull pidStr)
{
    decl_try_err();
    static char buf[PATH_MAX];
    procinfo_t info;
    int fd = -1;

    sprintf(buf, "/proc/%s", pidStr);

    try(open(buf, O_RDONLY, &fd));
    try(fiocall(fd, kProcCommand_GetInfo, &info));
    try(fiocall(fd, kProcCommand_GetName, buf, sizeof(buf)));

    const char* lastPathComponent = strrchr(buf, '/');
    const char* pnam = (lastPathComponent) ? lastPathComponent + 1 : buf;

    printf("%d  %s  %d  %zu\n", info.pid, pnam, info.ppid, info.virt_size);

catch:
    if (fd != -1) {
        close(fd);
    }

    return err;
}

static errno_t show_procs(void)
{
    decl_try_err();
    static dirent_t eb[4];
    int fd;

    err = opendir("/proc", &fd);
    if (err == EOK) {
        ssize_t nBytesRead;

        printf("PID  Command  PPID  Memory\n");

        while (err == EOK) {
            err = readdir(fd, eb, sizeof(eb), &nBytesRead);
            if (err != EOK || nBytesRead == 0) {
                break;
            }

            const dirent_t* dep = eb;
            while (nBytesRead > 0) {
                if (dep->name[0] != '.') {
                   err = show_proc(dep->name);
                    if (err != EOK) {
                        break;
                    }
                }

                nBytesRead -= sizeof(dirent_t);
                dep++;
            }
        }
    }

    close(fd);
    return err;
}


int main(int argc, char* argv[])
{
    decl_try_err();

    clap_parse(0, params, argc, argv);

    
    err = show_procs();
    if (err != EOK) {
        clap_error(argv[0], "%s", strerror(err));
    }

    return (err == EOK) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//
//  touch.c
//  cmds
//
//  Created by Dietmar Planitzer on 5/19/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <serena/directory.h>
#include <serena/file.h>


static bool touch_atim = false;
static bool touch_mtim = false;
static clap_string_array_t paths = {NULL, 0};

CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("touch [-a | --access] [-m | --modification] <path ...>"),

    CLAP_BOOL('a', "access", &touch_atim, "Just set a file's access time to the current time"),
    CLAP_BOOL('m', "modification", &touch_mtim, "Just set a file's modification time to the current time"),
    CLAP_REQUIRED_VARARG(&paths, "expected paths of files to touch")
);


static int touch(const char* _Nonnull path)
{
    nanotime_t times[2];

    times[FS_TIMFLD_ACC].tv_sec = 0;
    times[FS_TIMFLD_ACC].tv_nsec = FS_TIME_NOW;
    times[FS_TIMFLD_MOD].tv_sec = 0;
    times[FS_TIMFLD_MOD].tv_nsec = FS_TIME_NOW;

    if (touch_atim && !touch_mtim) {
        times[FS_TIMFLD_MOD].tv_nsec = FS_TIME_OMIT;
    }
    if (!touch_atim && touch_mtim) {
        times[FS_TIMFLD_ACC].tv_nsec = FS_TIME_OMIT;
    }


    if (fs_settimes(NULL, path, times) == 0) {
        // File existed and we successfully update its timestamp(s)
        return 0;
    }
    else if (errno == ENOENT) {
        // File does not exist. Create one
        int fd = fs_create_file(path, O_EXCL|O_RDWR, 0666);

        if (fd >= 0) {
            fd_close(fd);
            return 0;
        }
        else {
            return -1;
        }
    }
    else {
        return -1;
    }
}

int main(int argc, char* argv[])
{
    clap_parse(0, params, argc, argv);

    
    bool hasError = false;
    for (size_t i = 0; i < paths.count; i++) {
        const char* path = paths.strings[i];

        errno = 0;
        if (touch(path) != 0) {
            clap_error(argv[0], "%s: %s", path, strerror(errno));
            hasError = true;
        }
    }

    return (hasError) ? EXIT_FAILURE : EXIT_SUCCESS;
}

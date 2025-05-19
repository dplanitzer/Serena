//
//  main.c
//  systemd
//
//  Created by Dietmar Planitzer on 2/6/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/spawn.h>


static _Noreturn halt_machine(void)
{
    puts("Halting...");
    while (1);
    /* NOT REACHED */
}

static int start_proc(const char* _Nonnull procPath, const char* _Nonnull arg1)
{
    spawn_opts_t opts = {0};
    const char* argv[3];

    argv[0] = procPath;
    argv[1] = arg1;
    argv[2] = NULL;

    // Spawn the process
    return os_spawn(procPath, argv, &opts, NULL);
}

void main_closure(int argc, char *argv[])
{
    // Mount kernel object catalogs
    mount(kMount_Catalog, kCatalogName_Drivers, "/dev", "");
    mount(kMount_Catalog, kCatalogName_Filesystems, "/fs", "");
    mount(kMount_Catalog, kCatalogName_Processes, "/proc", "");


    // Startup login
    if (start_proc("/System/Commands/login", "/dev/console") != 0) {
        fputs("Error: ", stdout);
        puts(strerror(errno));
        halt_machine();
    }

    // Don't exit
}

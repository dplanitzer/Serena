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
#include <time.h>
#include <ext/nanotime.h>
#include <serena/clock.h>
#include <serena/filesystem.h>
#include <serena/process.h>
#include <serena/spawn.h>


static _Noreturn void halt_machine(void)
{
    puts("Halting...");
    while (1);
    /* NOT REACHED */
}

static int start_proc(const char* _Nonnull procPath, const char* _Nonnull arg1)
{
    proc_spawnattr_t attr;
    proc_spawn_actions_t actions;
    const char* argv[3];

    argv[0] = procPath;
    argv[1] = arg1;
    argv[2] = NULL;

    // Spawn the process. Note that we create an empty spawn actions array
    // because we do not want to pass any fds to our child process and thus we
    // need to make sure that the default behavior (sharing stdio) doesn't kick
    // in. 
    proc_spawnattr_init(&attr);
    proc_spawnattr_settype(&attr, SPAWN_SESSION_LEADER);
    proc_spawn_actions_init(&actions);

    const int r = proc_spawn(procPath, argv, NULL, &attr, &actions, NULL);
    
    proc_spawn_actions_destroy(&actions);
    proc_spawnattr_destroy(&attr);

    return r;
}

int main(int argc, char *argv[])
{
    // Mount kernel object catalogs
    fs_mount(FS_MOUNT_CATALOG, FS_CATALOG_DRIVERS, "/dev", "");


    // Startup login
    if (start_proc("/System/Commands/login", "/dev/console") != 0) {
        fputs("Error: ", stdout);
        puts(strerror(errno));
        halt_machine();
    }

    // Don't exit (doing it this cheep way for now. Should keep an eye on its children though)
    clock_wait(CLOCK_MONOTONIC, TIMER_ABSTIME, &NANOTIME_INF, NULL);
}

//
//  main.c
//  systemd
//
//  Created by Dietmar Planitzer on 2/6/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <System/System.h>


static _Noreturn halt_machine(void)
{
    puts("Halting...");
    while (1);
    /* NOT REACHED */
}

static errno_t start_proc(const char* _Nonnull procPath, const char* _Nonnull arg1)
{
    decl_try_err();
    SpawnOptions opts = {0};
    const char* argv[3];

    argv[0] = procPath;
    argv[1] = arg1;
    argv[2] = NULL;

    // Spawn the process
    try(Process_Spawn(procPath, argv, &opts, NULL));
    
catch:
    return err;
}

void main_closure(int argc, char *argv[])
{
    decl_try_err();

    
    // Mount devfs at /dev
    Mount(kMount_DriverCatalog, "drivers", "/dev", NULL, 0);


    // Startup login
    err = start_proc("/System/Commands/login", "/dev/console");
    if (err != EOK) {
        fputs("Error: ", stdout);
        puts(strerror(err));
        halt_machine();
    }

    // Don't exit
}

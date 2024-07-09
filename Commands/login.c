//
//  login.c
//  login
//
//  Created by Dietmar Planitzer on 7/8/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <System/System.h>


static char* _Nullable alloc_envar(const char* _Nonnull key, const char* _Nonnull value)
{
    const size_t keyLen = strlen(key);
    const size_t valLen = strlen(value);
    const size_t kvLen = keyLen + 1 + valLen;
    char* ep = malloc(kvLen + 1);

    if (ep) {
        memcpy(&ep[0], key, keyLen);
        ep[keyLen] = '=';
        memcpy(&ep[keyLen + 1], value, valLen);
    }
    return ep;
}

static errno_t run_shell(const char* _Nonnull shellPath, const char* _Nonnull homePath, int* _Nonnull pOutShellStatus)
{
    decl_try_err();
    ProcessId pid;
    ProcessTerminationStatus pts;
    SpawnOptions opts = {0};
    char* argv[1] = { NULL };
    char* envp[2] = { NULL, NULL };
    
    envp[0] = alloc_envar("HOME", homePath);
    opts.envp = envp;


    // Spawn the external command
    try(Process_Spawn(shellPath, argv, &opts, &pid));


    // Wait for the shell. The shell will not exit under normal circumstances
    try(Process_WaitForTerminationOfChild(pid, &pts));
    *pOutShellStatus = pts.status;
    
    char** ep = envp;
    while (*ep) {
        free(*ep);
        ep++;
    }

catch:
    return err;
}

void main_closure(int argc, char *argv[])
{
    const char* homePath = "/Users/Administrator";
    const char* shellPath = "/System/Commands/shell";
    int shell_status = 0, failed_count = 0;
    int fd;


    // Just exit if the console channels already exists which means that someone
    // invoked us after login
    if (IOChannel_GetMode(kIOChannel_Stdin) != 0) {
        exit(EXIT_FAILURE);
        /* NOT REACHED */
    }


    // Open the console and set up stdin, stdout and stderr
    File_Open("/dev/console", kOpen_Read, &fd);
    File_Open("/dev/console", kOpen_Write, &fd);
    File_Open("/dev/console", kOpen_Write, &fd);

    fdreopen(kIOChannel_Stdin, "r", stdin);
    fdreopen(kIOChannel_Stdout, "w", stdout);
    fdreopen(kIOChannel_Stderr, "w", stderr);


    // Make the user's home directory the current directory
    Process_SetWorkingDirectory(homePath);


    // Run the user's shell
    while (true) {
        const errno_t err = run_shell(shellPath, homePath, &shell_status);

        if (err == EOK) {
            failed_count = (shell_status != EXIT_SUCCESS) ? failed_count + 1 : 0;

            if (failed_count == 2) {
                printf("Error: unexpected shell termination: %d. Halting...", shell_status);
                while (1);
                /* NOT REACHED */
            }
        }
        else {
            printf("Error: %s. Halting...", strerror(err));
            while (1);
            /* NOT REACHED */
        }
    }

    /* NOT REACHED */
}

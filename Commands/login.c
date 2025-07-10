//
//  login.c
//  cmds
//
//  Created by Dietmar Planitzer on 7/8/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/dispatch.h>
#include <sys/spawn.h>
#include <sys/wait.h>
#include <sys/stat.h>

// Shut down the boot screen and initialize the kerne VT100 console
// XXX for now. Don't use outside of login.
extern int __coninit(void);


static void on_shell_termination(void* _Nullable ignore);


static int gFailedCounter;


static _Noreturn halt_machine(void)
{
    puts("Halting...");
    while (1);
    /* NOT REACHED */
}

static char* _Nullable env_alloc(const char* _Nonnull key, const char* _Nonnull value)
{
    const size_t keyLen = strlen(key);
    const size_t valLen = strlen(value);
    const size_t kvLen = keyLen + 1 + valLen;
    char* ep = malloc(kvLen + 1);

    if (ep) {
        memcpy(ep, key, keyLen);
        ep[keyLen] = '=';
        memcpy(&ep[keyLen + 1], value, valLen);
        ep[kvLen] = '\0';
    }
    return ep;
}

static int start_shell(const char* _Nonnull shellPath, const char* _Nonnull homePath)
{
    spawn_opts_t opts = {0};
    const char* argv[3];
    const char* envp[5];
    
    argv[0] = shellPath;
    argv[1] = "-l";
    argv[2] = NULL;

    envp[0] = env_alloc("HOME", homePath);
    envp[1] = env_alloc("SHELL", shellPath);
    envp[2] = env_alloc("TERM", "vt100");
    envp[3] = env_alloc("TMPDIR", "/tmp");
    envp[4] = NULL;

    opts.envp = envp;
    opts.umask = 0022;  // XXX hardcoded for now
    opts.uid = 1000;    // XXX hardcoded for now
    opts.gid = 1000;    // XXX hardcoded for now
    opts.notificationQueue = kDispatchQueue_Main;
    opts.notificationClosure = (os_dispatch_func_t)on_shell_termination;
    opts.notificationContext = NULL;
    opts.options = kSpawn_NewProcessGroup
        | kSpawn_NewSession
        | kSpawn_OverrideUserId
        | kSpawn_OverrideGroupId
        | kSpawn_OverrideUserMask
        | kSpawn_NotifyOnProcessTermination;


    // Spawn the shell
    const int r = os_spawn(shellPath, argv, &opts, NULL);


    char** ep = envp;
    while (*ep) {
        free(*ep);
        ep++;
    }
    
    return r;
}

// Log the user in. This means:
// - set the home directory to the user's directory
// - set up the environment variables
// - start the user's shell
static void login_user(void)
{
    const char* homePath = "/Users/admin";
    const char* shellPath = "/System/Commands/shell";

    puts("Logging in as admin...\n");

    // Make the current directory the user's home directory
    chdir(homePath);

    if (start_shell(shellPath, homePath) != 0) {
        printf("Error: %s.\n", strerror(errno));
        halt_machine();
    }
}

// Invoked by the kernel after the shell has terminated. We'll check whether the
// shell terminated with a success or failure status. We'll simply restart it in
// the first case and we'll halt the machine if the shell fails too often in a
// row.
static void on_shell_termination(void* _Nullable ignore)
{
    int child_stat;
    const pid_t child_pid = waitpid(-1, &child_stat, WNOHANG);

    if (child_pid > 0) {
        const int child_ec = WEXITSTATUS(child_stat);

        if (child_ec != EXIT_SUCCESS) {
            gFailedCounter++;
        }

        if (gFailedCounter == 2) {
            printf("Error: unexpected shell termination with status: %d.\n", child_ec);
            halt_machine();
        }
    }
    else {
        printf("Error: %s.\n", strerror(errno));
        halt_machine();
    }

    login_user();
}

// Invoked at app startup.
void main_closure(int argc, char *argv[])
{
    // login <termPath>
    if (argc < 2) {
        exit(EXIT_FAILURE);
        /* NOT REACHED */
    }
    const char* termPath = argv[1];


    // Just exit if the console channels already exist, which means that the
    // user is already logged in
    if (fcntl(STDIN_FILENO, F_GETFL) != -1) {
        exit(EXIT_FAILURE);
        /* NOT REACHED */
    }


    // XXX Temp. Fire up the kernel VT100 console
    __coninit();


    // Open the console and initialize stdin, stdout and stderr
    (void)open(termPath, O_RDONLY);
    (void)open(termPath, O_WRONLY);
    (void)open(termPath, O_WRONLY);

    fdreopen(STDIN_FILENO, "r", stdin);
    fdreopen(STDOUT_FILENO, "w", stdout);
    fdreopen(STDERR_FILENO, "w", stderr);


    printf("\033[36mSerena OS v0.5.0-alpha\033[0m\nCopyright 2023, Dietmar Planitzer.\n\n");

    
    // Log the user in and then return from our closure. Our VP will be moved
    // over to the shell and run the shell until it exits. Our 'on_shell_termination'
    // closure will get invoked when the shell exits.
    login_user();
}

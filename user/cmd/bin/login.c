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
#include <serena/file.h>
#include <serena/process.h>
#include <serena/signal.h>
#include <serena/spawn.h>
#include <serena/vcpu.h>


// Shut down the boot screen and initialize the kerne VT100 console
// XXX for now. Don't use outside of login.
extern int __coninit(void);


static void on_shell_termination(void* _Nullable ignore);


static int gFailedCounter;


static _Noreturn void halt_machine(void)
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
    proc_spawnattr_t sa;
    proc_spawnres_t sres;
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

    proc_spawnattr_init(&sa);
    proc_spawnattr_settype(&sa, SPAWN_GROUP_LEADER);
    proc_spawnattr_setumask(&sa, 0022); //XXX hardcoded for now
    proc_spawnattr_setuid(&sa, 1000);   //XXX hardcoded for now
    proc_spawnattr_setgid(&sa, 1000);   //XXX hardcoded for now

    
    // Spawn the shell
    const int r = proc_spawn(shellPath, argv, envp, &sa, NULL, &sres);


    proc_spawnattr_destroy(&sa);

    char** ep = envp;
    while (*ep) {
        free(*ep);
        ep++;
    }
    
    // XXX enable dispatch queue based notifications again
    // XXX broken for now. Typing exit in the login shell will throw an error
    // XXX because this proc_waitstate() here consumes the pid and the proc_waitstate()
    // XXX in on_shell_termination() can't get the pid anymore.
    proc_waitres_t ps;
    proc_waitstate(WAIT_FOR_TERMINATED, WAIT_PID, sres.pid, 0, &ps);
    on_shell_termination(NULL);
    // XXX enable dispatch queue based notifications again

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
    proc_setcwd(homePath);

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
    proc_waitres_t ps;
    const int r = proc_waitstate(WAIT_FOR_TERMINATED, WAIT_ANY, 0, WAIT_NONBLOCKING, &ps);

    if (r == -1) {
        printf("Error: %s.\n", strerror(errno));
        halt_machine();
        /* NOT REACHED */
    }

    if ((ps.reason == WAIT_REASON_EXITED && ps.u.status != EXIT_SUCCESS) || ps.reason != WAIT_REASON_EXITED) {
        gFailedCounter++;
    }

    if (gFailedCounter == 2) {
        printf("Error: unexpected shell (%d) termination with status: %d:%d.\n", ps.pid, ps.reason, ps.u.status);
        halt_machine();
        /* NOT REACHED */
    }

    login_user();
}

int main(int argc, char *argv[])
{
    // login <termPath>
    if (argc < 2) {
        exit(EXIT_FAILURE);
        /* NOT REACHED */
    }
    const char* termPath = argv[1];


    // Just exit if the console channels already exist, which means that the
    // user is already logged in
    if (fd_type(FD_STDIN) > FD_TYPE_INVALID) {
        exit(EXIT_FAILURE);
        /* NOT REACHED */
    }


    // XXX Temp. Fire up the kernel VT100 console
    __coninit();


    // Open the console and initialize stdin, stdout and stderr
    (void)fs_open(NULL, termPath, O_RDONLY | O_PRSVEXEC);
    (void)fs_open(NULL, termPath, O_WRONLY | O_PRSVEXEC);
    (void)fs_open(NULL, termPath, O_WRONLY | O_PRSVEXEC);

    fdreopen(FD_STDIN, "r", stdin);
    fdreopen(FD_STDOUT, "w", stdout);
    fdreopen(FD_STDERR, "w", stderr);


    // Enable SIG_CHILD reception
    sig_route(SIG_ROUTE_ADD, SIG_CHILD, SIG_SCOPE_VCPU, VCPUID_MAIN);
    

    printf("\033[36mSerena OS v0.8.0-alpha\033[0m\nCopyright 2023 - 2026, Dietmar Planitzer.\n\n");

    
    // Log the user in and then return from our closure. Our VP will be moved
    // over to the shell and run the shell until it exits. Our 'on_shell_termination'
    // closure will get invoked when the shell exits.
    login_user();
}

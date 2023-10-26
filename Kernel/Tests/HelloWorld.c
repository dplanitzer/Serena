//
//  HelloWorld.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/9/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <syscall.h>
#include <apollo/apollo.h>

////////////////////////////////////////////////////////////////////////////////
// Process with a Child Process
////////////////////////////////////////////////////////////////////////////////

#if 0
int count1;
int count2;

static void parent_process(void)
{
    printf("Hello World, from process #1!  [%d]\n", count1++);
    __syscall(SC_sleep, 0, 250*1000*1000);
    __syscall(SC_dispatch_async, (void*)parent_process);
}

static void child_process(void)
{
    printf("Hello World, from process #2!          [%d]\n", count2++);
    __syscall(SC_sleep, 1, 0*1000*1000);
    __syscall(SC_dispatch_async, (void*)child_process);
    //exit(0);
    //puts("oops\n");
}


void app_main(int argc, char *argv[])
{
    printf(" pid: %d\nargc: %d\n", getpid(), argc);
    for (int i = 0; i < argc; i++) {
        if (argv[i]) {
            puts(argv[i]);
        }
    }
    putchar('\n');

    if (argc == 0) {
        // Parent process.
        
        // Spawn a child process
        char* child_argv[2];
        child_argv[0] = "--child";
        child_argv[1] = NULL;

        spawn_arguments_t spargs;
        spargs.execbase = (void*)0xfe0000;
        spargs.argv = child_argv;
        spargs.envp = NULL;
        spawnp(&spargs);

        // Do a parent's work
        parent_process();
    } else {
        // Child process
        printf("ppid: %d\n\n", getppid());
        child_process();
    }
}

#endif


////////////////////////////////////////////////////////////////////////////////
// Interactive Console
////////////////////////////////////////////////////////////////////////////////

#if 1
void app_main(int argc, char *argv[])
{
    printf("Console v1.0\nReady.\n\n");

    while (true) {
        const int ch = getchar();
        if (ch == EOF) {
            printf("Read error\n");
            continue;
        }

        putchar(ch);
        //printf("0x%hhx\n", ch);
    }
}

#endif


////////////////////////////////////////////////////////////////////////////////
// Common startup
////////////////////////////////////////////////////////////////////////////////

void main_closure(int argc, char *argv[])
{
    assert(open("/dev/console", O_RDONLY) == 0);
    assert(open("/dev/console", O_WRONLY) == 1);

    app_main(argc, argv);
}

//
//  ProcessTests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 7/9/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <apollo/apollo.h>

////////////////////////////////////////////////////////////////////////////////
// Process with a child process
////////////////////////////////////////////////////////////////////////////////

int cpt_count1;
int cpt_count2;

static void parent_process(void)
{
    struct timespec delay = {0, 250*1000*1000};
    printf("Hello World, from process #1!  [%d]\n", cpt_count1++);
    nanosleep(&delay);
    __syscall(SC_dispatch_async, (void*)parent_process);
}

static void child_process(void)
{
    struct timespec delay = {1, 0*1000*1000};
    printf("Hello World, from process #2!          [%d]\n", cpt_count2++);
    nanosleep(&delay);
    __syscall(SC_dispatch_async, (void*)child_process);
    //exit(0);
    //puts("oops\n");
}


void child_process_test(int argc, char *argv[])
{
    printf(" pid: %ld\nargc: %d\n", getpid(), argc);
    
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
        memset(&spargs, 0, sizeof(spargs));
        spargs.execbase = (void*)0xfe0000;
        //spargs.execbase = (void*)(0xfe0000 + ((char*)app_main));
        spargs.argv = child_argv;
        spargs.envp = NULL;
        spawnp(&spargs, NULL);

        // Do a parent's work
        parent_process();
    } else {
        // Child process
        printf("ppid: %ld\n\n", getppid());
        child_process();
    }
}

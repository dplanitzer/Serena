//
//  HelloWorld.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/9/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <syscall.h>

////////////////////////////////////////////////////////////////////////////////
// Process with a Child Process
////////////////////////////////////////////////////////////////////////////////

#if 1
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


void main_closure(int argc, char *argv[])
{
    printf("argc: %d\n", argc);
    for (int i = 0; i < argc; i++) {
        if (argv[i]) {
            puts(argv[i]);
        }
    }
    putchar('\n');
    putchar('\n');

    if (argc == 0) {
        // Parent process.
        
        // Spawn a child process
        char* child_argv[2];
        child_argv[0] = "--child";
        child_argv[1] = NULL;

        __syscall(SC_spawn_process, (void*)0xfe0000, child_argv, NULL);

        // Do a parent's work
        parent_process();
    } else {
        // Child process
        child_process();
    }
}

#endif


////////////////////////////////////////////////////////////////////////////////
// Interactive Console
////////////////////////////////////////////////////////////////////////////////

#if 0
void main_closure(int argc, char *argv[])
{
    printf("Console v1.0\nReady.\n\n");

    while (true) {
        const int ch = getchar();
        if (ch == EOF) {
            printf("Read error\n");
            continue;
        }

        putchar(ch);
    }
}

#endif

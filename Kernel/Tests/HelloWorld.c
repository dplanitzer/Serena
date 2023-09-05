//
//  HelloWorld.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/9/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <syscall.h>

// Switch between a standard C style main() and an Apollo style main_closure()
// process startup behavior. Also change projectmk to use LIBC_CSTART_FILE for
// standard C behavior and LIBC_ASTART_FILE for Apollo behavior.
//#define STDC_MAIN

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


#ifdef STDC_MAIN
int main(int argc, char *argv[])
#else
void main_closure(int argc, char *argv[])
#endif
{
    __syscall(SC_spawn_process, (void*)child_process);

#ifdef STDC_MAIN
    while (1) {
        parent_process();
    }

    return EXIT_SUCCESS;
#else
    parent_process();
#endif
}

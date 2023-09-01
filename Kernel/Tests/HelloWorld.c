//
//  HelloWorld.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/9/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <System.h>
#include <stdlib.h>
#include <stdio.h>

// Switch between a standard C style main() and an Apollo style main_closure()
// process startup behavior. Also change projectmk to use CLIB_CSTART_FILE for
// standard C behavior and CLIB_ASTART_FILE for Apollo behavior.
//#define STDC_MAIN

int count1;
int count2;

static void parent_process(void)
{
    printf("Hello World, from process #1!  [%d]\n", count1++);
    __sleep(0, 250*1000*1000);
    __dispatch_async((void*)parent_process);
}

static void child_process(void)
{
    printf("Hello World, from process #2!          [%d]\n", count2++);
    __sleep(0, 250*1000*1000);
    __dispatch_async((void*)child_process);
    //exit(0);
    //puts("oops\n");
}


#ifdef STDC_MAIN
int main(int argc, char *argv[])
#else
void main_closure(int argc, char *argv[])
#endif
{
    __spawn_process((void*)child_process);

#ifdef STDC_MAIN
    while (1) {
        parent_process();
    }

    return EXIT_SUCCESS;
#else
    parent_process();
#endif
}

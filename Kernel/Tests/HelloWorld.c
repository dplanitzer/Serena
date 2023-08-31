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

int count1;
int count2;

static void child_process(void)
{
    printf("Hello World, from process #2!          [%d]\n", count2++);
    __sleep(0, 250*1000*1000);
    __dispatch_async((void*)child_process);
    //exit(0);
    //puts("oops\n");
}

int main(int argc, char *argv[])
{
    __spawn_process((void*)child_process);

    while (1) {
        printf("Hello World, from process #1!  [%d]\n", count1++);
        __sleep(0, 250*1000*1000);
    }

    return EXIT_SUCCESS;
}

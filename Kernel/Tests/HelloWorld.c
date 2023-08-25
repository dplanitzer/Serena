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

static void helloWorld_a(void)
{
    printf("Hello World, from process #1!\n");
    __sleep(0, 250*1000*1000);
    __dispatch_async((void*)helloWorld_a);
    //exit(0);
    //puts("oops\n");
}

static void helloWorld_b(void)
{
    printf("Hello World, from process #2!\n");
    __sleep(0, 250*1000*1000);
    __dispatch_async((void*)helloWorld_b);
    //exit(0);
    //puts("oops\n");
}

int main(void)
{
    helloWorld_a();
    __spawn_process((void*)helloWorld_b);

    return 0;
}

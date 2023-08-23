//
//  HelloWorld.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/9/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <System.h>


static void helloWorld_a(void)
{
    write("Hello World, from process #1!\n");
    sleep(0, 250*1000*1000);
    dispatch_async((void*)helloWorld_a);
    //exit(0);
    //write("oops\n");
}

static void helloWorld_b(void)
{
    write("Hello World, from process #2!\n");
    sleep(0, 250*1000*1000);
    dispatch_async((void*)helloWorld_b);
    //exit(0);
    //write("oops\n");
}

int main(void)
{
    helloWorld_a();
    spawn_process((void*)helloWorld_b);

    return 0;
}

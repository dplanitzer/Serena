//
//  HelloWorld.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/9/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <System.h>


static void helloWorld(void)
{
    write("Hello World from user space!\n");
    sleep(0, 250*1000*1000);
    dispatch_async((void*)helloWorld);
    //exit(0);
    //write("oops\n");
}

int main(void)
{
    helloWorld();
    return 0;
}

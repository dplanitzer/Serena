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
    print("Hello World from user space!\n");
    sleep(0, 250*1000*1000);
    dispatchAsync((void*)helloWorld);
}

int main(void)
{
    helloWorld();
    return 0;
}

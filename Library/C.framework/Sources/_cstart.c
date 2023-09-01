//
//  _cstart.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/31/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__stddef.h>
#include <stdlib.h>
#include <System.h>


extern int main(int argc, char *argv[]);

// start() that implements the standard C semantics.
void start(void)
{
    __stdlibc_init();

    char* argv[1];
    argv[0] = NULL;

    exit(main(0, argv));
}

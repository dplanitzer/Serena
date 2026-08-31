//
//  cursor_tests.c
//  libflowterm Tests
//
//  Created by Dietmar Planitzer on 8/30/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <flowterm.h>
#include <errno.h>
#include <stdbool.h>


void curpos_test(int argc, char *argv[])
{
    int x, y;

    ft_curpos(&x, &y);
    printf("x: %d, y: %d\n", x, y);
}

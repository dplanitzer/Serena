//
//  query_tests.c
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

void screensize_test(int argc, char *argv[])
{
    int w, h;

    ft_screensize(&w, &h);
    printf("w: %d, h: %d\n", w, h);
}

// Expected: 0, 0
void status_test(int argc, char *argv[])
{
    errno = 0;
    printf("status: %d, errno: %d\n", ft_status(), errno);
}

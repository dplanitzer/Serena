//
//  ConsoleTests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 7/9/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <ctype.h>
#include <stdbool.h>
#define _OPEN_SYS_ITOA_EXT
#include <stdlib.h>
#include <stdio.h>


void interactive_console_test(int argc, char *argv[])
{
    char buf[4];

    puts("End with 'q'");

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    while (true) {
        const int ch = getchar();

        if (ch == EOF || ch == 'q') {
            printf("Done\n");
            break;
        }

        if (isprint(ch)) {
            putchar(ch);
        }
        else {
            fputs("0x", stdout);
            fputs(utoa(ch, buf, 16), stdout);
        }
        putchar('\n');
    }
}

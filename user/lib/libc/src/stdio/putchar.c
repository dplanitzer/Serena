//
//  putchar.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


int putchar(int ch)
{
    return putc(ch, stdout);
}

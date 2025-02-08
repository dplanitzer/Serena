//
//  tmpnam.c
//  libc
//
//  Created by Dietmar Planitzer on 2/7/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>


char *tmpnam(char *filename)
{
    static char gTmpnamBuffer[L_tmpnam];

    return tmpnam_r((filename) ? filename : gTmpnamBuffer);
}

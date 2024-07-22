//
//  rewind.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


void rewind(FILE *s)
{
    (void) fseek(s, 0, SEEK_SET);
    clearerr(s);
    // XXX drop ungetc buffered stuff
}

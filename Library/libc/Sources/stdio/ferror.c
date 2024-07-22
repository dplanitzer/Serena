//
//  ferror.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


void clearerr(FILE *s)
{
    s->flags.hasError = 0;
    s->flags.hasEof = 0;
}

int feof(FILE *s)
{
    return (s->flags.hasEof) ? EOF : 0;
}

int ferror(FILE *s)
{
    return (s->flags.hasError) ? EOF : 0;
}

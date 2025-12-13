//
//  rewind.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


void rewind(FILE * _Nonnull s)
{
    (void) fseek(s, 0, SEEK_SET);
    s->flags.hasError = 0;
}

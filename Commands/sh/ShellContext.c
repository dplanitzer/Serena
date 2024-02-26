//
//  ShellContext.c
//  sh
//
//  Created by Dietmar Planitzer on 2/25/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ShellContext.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


errno_t ShellContext_Create(LineReaderRef _Nullable pLineReader, ShellContextRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ShellContextRef self;
    
    try_null(self, calloc(1, sizeof(ShellContext)), ENOMEM);
    self->lineReader = pLineReader;

    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

void ShellContext_Destroy(ShellContextRef _Nullable self)
{
    if (self) {
        self->lineReader = NULL;
        free(self);
    }
}

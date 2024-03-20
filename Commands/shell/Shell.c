//
//  Shell.c
//  sh
//
//  Created by Dietmar Planitzer on 2/25/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Shell.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


errno_t Shell_CreateInteractive(ShellRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ShellRef self;
    
    try_null(self, calloc(1, sizeof(Shell)), ENOMEM);
    try(LineReader_Create(79, 10, ">", &self->lineReader));
    try(ShellContext_Create(self->lineReader, &self->context));
    try(Parser_Create(&self->parser));
    try(Interpreter_Create(self->context, &self->interpreter));

    *pOutSelf = self;
    return EOK;

catch:
    Shell_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void Shell_Destroy(ShellRef _Nullable self)
{
    if (self) {
        Interpreter_Destroy(self->interpreter);
        self->interpreter = NULL;
        Parser_Destroy(self->parser);
        self->parser = NULL;
        ShellContext_Destroy(self->context);
        self->context = NULL;
        LineReader_Destroy(self->lineReader);
        self->lineReader = NULL;
        free(self);
    }
}

errno_t Shell_Run(ShellRef _Nonnull self)
{
    decl_try_err();
    ScriptRef pScript = NULL;

    if (self->lineReader == NULL) {
        throw(EINVAL);
    }
    try(Script_Create(&pScript));

    while (true) {
        char* line = LineReader_ReadLine(self->lineReader);

        putchar('\n');
        
        Parser_Parse(self->parser, line, pScript);
        Interpreter_Execute(self->interpreter, pScript);
    }
    return EOK;

catch:
    Script_Destroy(pScript);
    return err;
}

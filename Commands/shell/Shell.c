//
//  Shell.c
//  sh
//
//  Created by Dietmar Planitzer on 2/25/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Shell.h"
#include "Errors.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


errno_t Shell_Create(bool isInteractive, ShellRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ShellRef self;
    
    try_null(self, calloc(1, sizeof(Shell)), ENOMEM);
    if (isInteractive) {
        try(LineReader_Create(79, 10, ">", &self->lineReader));
    }
    try(Parser_Create(&self->parser));
    try(Interpreter_Create(self->lineReader, &self->interpreter));

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
        LineReader_Destroy(self->lineReader);
        self->lineReader = NULL;
        free(self);
    }
}

static void _Shell_ExecuteString(ShellRef _Nonnull self, const char* _Nonnull str, Script* _Nonnull script)
{
    decl_try_err();

    Script_Reset(script);

    err = Parser_Parse(self->parser, str, script);
    if (err == EOK) {
        err = Interpreter_Execute(self->interpreter, script);
    }

    if (err != EOK) {
        printf("Error: %s.\n", shell_strerror(err));
    }
}

errno_t Shell_Run(ShellRef _Nonnull self)
{
    decl_try_err();
    Script* script = NULL;

    if (self->lineReader == NULL) {
        throw(EINVAL);
    }
    try(Script_Create(&script));

    while (true) {
        char* line = LineReader_ReadLine(self->lineReader);

        putchar('\n');
        _Shell_ExecuteString(self, line, script);
    }

catch:
    Script_Destroy(script);
    return err;
}

errno_t Shell_RunContentsOfFile(ShellRef _Nonnull self, const char* _Nonnull path)
{
    decl_try_err();
    Script* script = NULL;
    FILE* fp = NULL;
    char* text = NULL;

    try(Script_Create(&script));
    try_null(fp, fopen(path, "r"), errno);
    fseek(fp, 0, SEEK_END);
    const size_t fileSize = ftell(fp);
    rewind(fp);

    if (fileSize > 0ll) {
        text = malloc(fileSize + 1);
        fread(text, fileSize, 1, fp);
        if (ferror(fp)) {
            throw(errno);
        }
        text[fileSize] = '\0';

        _Shell_ExecuteString(self, text, script);
    }

catch:
    free(text);
    fclose(fp);
    Script_Destroy(script);
    return err;
}

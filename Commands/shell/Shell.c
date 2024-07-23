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

static void _Shell_ExecuteString(ShellRef _Nonnull self, const char* _Nonnull str, Script* _Nonnull script, bool bPushScope)
{
    decl_try_err();

    Script_Reset(script);

    err = Parser_Parse(self->parser, str, script);
    if (err == EOK) {
        err = Interpreter_Execute(self->interpreter, script, bPushScope);
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
        _Shell_ExecuteString(self, line, script, false);    // No script scope in interactive mode
    }

catch:
    Script_Destroy(script);
    return err;
}

static errno_t read_contents_of_file(const char* _Nonnull path, char* _Nullable * _Nonnull pOutText)
{
    decl_try_err();
    FILE* fp = NULL;
    char* text = NULL;

    try_null(fp, fopen(path, "r"), errno);
    fseek(fp, 0, SEEK_END);
    const size_t fileSize = ftell(fp);
    rewind(fp);

    try_null(text, malloc(fileSize + 1), errno);
    if (fileSize > 0ll) {
        fread(text, fileSize, 1, fp);
        if (ferror(fp)) {
            throw(errno);
        }
    }
    text[fileSize] = '\0';
    fclose(fp);

    *pOutText = text;
    return EOK;

catch:
    free(text);
    fclose(fp);

    *pOutText = NULL;
    return err;
}

errno_t Shell_RunContentsOfFile(ShellRef _Nonnull self, const char* _Nonnull path)
{
    decl_try_err();
    Script* script = NULL;
    char* text = NULL;

    try(Script_Create(&script));
    try(read_contents_of_file(path, &text));
    _Shell_ExecuteString(self, text, script, true);

catch:
    free(text);
    Script_Destroy(script);
    return err;
}

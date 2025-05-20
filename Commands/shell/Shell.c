//
//  Shell.c
//  sh
//
//  Created by Dietmar Planitzer on 2/25/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Shell.h"
#include "Errors.h"
#include "Utilities.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


ShellRef _Nonnull Shell_Create(bool isInteractive)
{
    ShellRef self = calloc(1, sizeof(Shell));

    if (isInteractive) {
        self->lineReader = LineReader_Create(79, 10, ">");
    }
    self->parser = Parser_Create();
    self->interpreter = Interpreter_Create(self->lineReader);

    return self;
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

static void _Shell_ExecuteString(ShellRef _Nonnull self, const char* _Nonnull str, Script* _Nonnull script, bool isInteractive)
{
    decl_try_err();
    const ExecuteOptions options = (isInteractive) ? kExecute_Interactive : kExecute_PushScope;

    Script_Reset(script);

    err = Parser_Parse(self->parser, str, script);
    if (err == EOK) {
        err = Interpreter_Execute(self->interpreter, script, options);
    }

    if (err != EOK) {
        printf("Error: %s.\n", shell_strerror(err));
    }
}

errno_t Shell_Run(ShellRef _Nonnull self)
{
    if (self->lineReader == NULL) {
        return EINVAL;
    }

    Script* script = Script_Create();

    while (true) {
        char* line = LineReader_ReadLine(self->lineReader);

        putchar('\n');
        _Shell_ExecuteString(self, line, script, true);    // No script scope in interactive mode
    }

    Script_Destroy(script);
    return EOK;
}

errno_t Shell_RunContentsOfString(ShellRef _Nonnull self, const char* _Nonnull str)
{
    Script* script = Script_Create();

    _Shell_ExecuteString(self, str, script, false);

    Script_Destroy(script);
    return EOK;
}

errno_t Shell_RunContentsOfFile(ShellRef _Nonnull self, const char* _Nonnull path)
{
    decl_try_err();
    char* text = NULL;

    Script* script = Script_Create();
    try(read_contents_of_file(path, &text, NULL));
    _Shell_ExecuteString(self, text, script, false);

catch:
    free(text);
    Script_Destroy(script);
    return err;
}

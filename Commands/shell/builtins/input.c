//
//  input.c
//  sh
//
//  Created by Dietmar Planitzer on 8/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "LineReader.h"
#include "Utilities.h"
#include <stdlib.h>
#include <clap.h>


static const char* prompt = "";

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("input [prompt]"),

    CLAP_POSITIONAL_STRING(&prompt)
);


static errno_t do_input(InterpreterRef _Nonnull ip)
{
    decl_try_err();
    LineReaderRef lineReader = NULL;

    // XXX figure out what to do about the max length. I.e. should probably be controllable with an argument
    err = LineReader_Create(40, 0, prompt, &lineReader);
    if (err == EOK) {
        err = OpStack_PushCString(ip->opStack, LineReader_ReadLine(lineReader));
        
        if (ip->isInteractive) {
            putchar('\n');
        }
    }
    else {
        err = OpStack_PushVoid(ip->opStack);
    }

    LineReader_Destroy(lineReader);
    return err;
}

int cmd_input(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode;

    if (!clap_should_exit(status)) {
        exitCode = exit_code(do_input(ip));
    }
    else {
        exitCode = clap_exit_code(status);
        OpStack_PushVoid(ip->opStack);
    }
    
    return exitCode;
}

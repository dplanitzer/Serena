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
#include <System/IOChannel.h>


static const char* prompt = "";

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("input [prompt]"),

    CLAP_POSITIONAL_STRING(&prompt)
);


int cmd_input(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    decl_try_err();
    LineReaderRef lineReader = NULL;

    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    if (clap_should_exit(status)) {
        return clap_exit_code(status);
    }    
    
    // XXX figure out what to do about the max length. I.e. should probably be controllable with an argument
    try(LineReader_Create(40, 0, prompt, &lineReader));
    char* line = LineReader_ReadLine(lineReader);

    if (IOChannel_GetType(kIOChannel_Stdout) == kIOChannelType_Terminal) {
        printf("\n%s\n", line);
    }
    else {
        fputs(line, stdout);
    }

catch:
    LineReader_Destroy(lineReader);
    return exit_code(err);
}

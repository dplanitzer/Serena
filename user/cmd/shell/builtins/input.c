//
//  input.c
//  sh
//
//  Created by Dietmar Planitzer on 8/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <readline.h>
#include <stdlib.h>
#include <clap.h>


static const char* prompt = "";

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("input [prompt]"),

    CLAP_POSITIONAL_STRING(&prompt)
);


static void do_input(InterpreterRef _Nonnull ip)
{
    // XXX figure out what to do about the max length. I.e. should probably be controllable with an argument
    rl_t lineReader = rl_create(0, RL_SCREEN_WIDTH); 
    rl_setprompt(lineReader, prompt);
    
    OpStack_PushCString(ip->opStack, rl_readline(lineReader));
        
    if (ip->isInteractive) {
        putchar('\n');
    }

    rl_destroy(lineReader);
}

int cmd_input(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode = EXIT_SUCCESS;

    if (!clap_should_exit(status)) {
        do_input(ip);
    }
    else {
        exitCode = clap_exit_code(status);
        OpStack_PushVoid(ip->opStack);
    }
    
    return exitCode;
}

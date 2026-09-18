//
//  shutdown.c
//  sh
//
//  Created by Dietmar Planitzer on 4/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <stdio.h>
#include <serena/filesystem.h>
#include <clap.h>


static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("shutdown")
);


static void do_pwd(InterpreterRef _Nonnull ip, const char* _Nonnull proc_name)
{
    fs_sync();
    
    fputs("It is now safe to turn power to your computer off.\n", stdout);
    fputs("\033[?25l", stdout);

    for (;;) {
        // do nothing
    }
}

int cmd_shutdown(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode = EXIT_SUCCESS;

    if (!clap_should_exit(status)) {
        do_pwd(ip, argv[0]);
    }
    else {
        exitCode = clap_exit_code(status);
        OpStack_PushVoid(ip->opStack);
    }

    return exitCode;
}

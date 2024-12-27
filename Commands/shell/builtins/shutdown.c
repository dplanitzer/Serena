//
//  shutdown.c
//  sh
//
//  Created by Dietmar Planitzer on 12/26/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <clap.h>
#include <System/Disk.h>


static const char* path;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("shutdown")
);


static errno_t do_shutdown(void)
{
    Sync();
    
    fputs("It is now safe to turn power off.\n", stdout);
    fputs("\033[?25l", stdout);

    for (;;) {
        // do nothing
    }
    
    return EOK;
}

int cmd_shutdown(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode;

    if (!clap_should_exit(status)) {
        exitCode = exit_code(do_shutdown());
    }
    else {
        exitCode = clap_exit_code(status);
    }

    OpStack_PushVoid(ip->opStack);
    return exitCode;
}

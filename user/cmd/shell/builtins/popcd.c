//
//  popcd.c
//  sh
//
//  Created by Dietmar Planitzer on 4/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <clap.h>


static const char* path;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("popcd")
);


static int do_popcd(InterpreterRef _Nonnull ip, const char* _Nonnull proc_name)
{
    if (ip->cdStackTos) {
        if (chdir(ip->cdStackTos->path) == 0) {
            CDEntry* ep = ip->cdStackTos;

            ip->cdStackTos = ep->prev;
            free(ep->path);
            free(ep);
        }

        return EXIT_SUCCESS;
    }
    else {
        fprintf(stderr, "%s: empty stack\n", proc_name);
        return EXIT_FAILURE;
    }
}

int cmd_popcd(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode;

    if (!clap_should_exit(status)) {
        exitCode = do_popcd(ip, argv[0]);
    }
    else {
        exitCode = clap_exit_code(status);
    }

    OpStack_PushVoid(ip->opStack);
    return exitCode;
}

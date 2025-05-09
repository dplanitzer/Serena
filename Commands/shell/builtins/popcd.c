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
#include <clap.h>


static const char* path;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("popcd")
);


static errno_t do_popcd(InterpreterRef _Nonnull ip, const char* proc_name)
{
    decl_try_err();

    if (ip->cdStackTos) {
        err = setcwd(ip->cdStackTos->path);
        if (err == EOK) {
            CDEntry* ep = ip->cdStackTos;

            ip->cdStackTos = ep->prev;
            free(ep->path);
            free(ep);
        }
    }
    else {
        fprintf(stderr, "%s: empty stack\n", proc_name);
    }

    return err;
}

int cmd_popcd(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode;

    if (!clap_should_exit(status)) {
        exitCode = exit_code(do_popcd(ip, argv[0]));
    }
    else {
        exitCode = clap_exit_code(status);
    }

    OpStack_PushVoid(ip->opStack);
    return exitCode;
}

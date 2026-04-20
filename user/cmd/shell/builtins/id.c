//
//  id.c
//  sh
//
//  Created by Dietmar Planitzer on 4/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include <clap.h>
#include <stdio.h>
#include <stdlib.h>
#include <serena/process.h>


static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("id")
);


int cmd_id(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    proc_basic_info_t info;

    clap_parse(0, params, argc, argv);

    (void)proc_info(PROC_SELF, PROC_INFO_BASIC, &info);
    printf("uid=%u, gid=%u\n", info.uid, info.gid);

    OpStack_PushVoid(ip->opStack);
    return EXIT_SUCCESS;
}

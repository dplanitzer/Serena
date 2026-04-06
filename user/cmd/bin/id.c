//
//  id.c
//  cmds
//
//  Created by Dietmar Planitzer on 4/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <stdio.h>
#include <stdlib.h>
#include <serena/process.h>


CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("id")
);


int main(int argc, char* argv[])
{
    proc_creds_info_t info;

    clap_parse(0, params, argc, argv);

    (void)proc_info(PROC_SELF, PROC_INFO_CREDS, &info);
    printf("uid=%u, gid=%u\n", info.uid, info.gid);

    return EXIT_SUCCESS;
}

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
#include <System/Process.h>


CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("id")
);


int main(int argc, char* argv[])
{
    clap_parse(0, params, argc, argv);

    
    printf("uid=%u, gid=%u\n", getuid(), getgid());

    return EXIT_SUCCESS;
}

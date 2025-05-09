//
//  shutdown.c
//  cmds
//
//  Created by Dietmar Planitzer on 4/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <stdio.h>
#include <System/Disk.h>


CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("shutdown")
);


int main(int argc, char* argv[])
{
    clap_parse(0, params, argc, argv);

    
    sync();
    
    fputs("It is now safe to turn power to your computer off.\n", stdout);
    fputs("\033[?25l", stdout);

    for (;;) {
        // do nothing
    }

    return EXIT_SUCCESS;
}

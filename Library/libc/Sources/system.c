//
//  system.c
//  libc
//
//  Created by Dietmar Planitzer on 2/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>

// Return 0 for "no command processor" (for now anyway)
int system(const char *string)
{
    if (string == NULL) {
        return 0;
    }

    return EXIT_FAILURE;
}

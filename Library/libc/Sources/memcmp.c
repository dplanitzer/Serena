//
//  memcmp.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>


int memcmp(const void *lhs, const void *rhs, size_t count)
{
    unsigned char *plhs = (unsigned char *)lhs;
    unsigned char *prhs = (unsigned char *)rhs;
    
    while (count-- > 0) {
        if (*plhs++ != *prhs++) {
            return ((int)*plhs) - ((int)*prhs);
        }
    }

    return 0;
}

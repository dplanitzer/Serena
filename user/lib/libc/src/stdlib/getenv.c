//
//  getenv.c
//  libc
//
//  Created by Dietmar Planitzer on 9/15/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <__stdlib.h>


// Like getenv() but also returns the index of the 'name' entry in the environ
// table. Returns NULL and an index of -1 if 'name' was not found. Note that at
// most 'nMaxChars' are considered when comparing 'name' against the entries
// stored in the table.
char* _Nullable __getenv(const char *_Nonnull name, size_t nMaxChars, ssize_t* _Nonnull idx)
{
    if (name) {
        const char* const * p = (const char * const *) environ;

        while (*p) {
            const char* vname = *p;

            if (!strncmp(name, vname, nMaxChars) && vname[nMaxChars] == '=') {
                *idx = p - environ;
                return (char*) &vname[nMaxChars + 1];
            }
            p++;
        }
    }

    *idx = -1;
    return NULL;
}



char *getenv(const char *name)
{
    ssize_t idx;
    return __getenv(name, strlen(name), &idx);
}

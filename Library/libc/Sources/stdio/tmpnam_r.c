//
//  tmpnam_r.c
//  libc
//
//  Created by Dietmar Planitzer on 2/7/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <System/File.h>

#define NUM_RND_CHARS   16


static void __generate_rnd_chars(char* buf, unsigned int seed)
{
    unsigned int rnd_state = seed;

    // a random character is one of:
    // a - z
    // A - Z
    // 0 - 9
    // -> 62 possible choices per character
    for (int i = 0; i < NUM_RND_CHARS; i++) {
        int r;
        char ch;

        // Do it this way to maintain a uniform distribution
        do {
            r = rand_r(&rnd_state);
        } while (r >= 62);
        

        if (r < 26) {
            ch = 'a' + r;
        }
        else if (r < 52) {
            ch = 'A' + r;
        }
        else {
            ch = '0' + r;
        }


        buf[i] = ch;
    }
}

char *__tmpnam_r(char *filename, int* pOutIoc)
{
    if (filename == NULL) {
        return NULL;
    }

    char* ep = &filename[L_tmpnam - 1];
    char* p = filename;


    // Get the directory
    const char* dir = getenv("TMPDIR");
    if (dir == NULL || *dir == '\0') {
        dir = P_tmpdir;
    }


    // Verify that the directory exists
    if (access(dir, R_OK | X_OK) != 0) {
        return NULL;
    }


    // Write the directory with a trailing slash to the buffer
    const size_t dirLen = strlen(dir);
    const bool hasTrailingSlash = (dir[dirLen - 1] == '/') ? true : false;
    if (p + dirLen > ep) {
        return NULL;
    }

    memcpy(p, dir, dirLen);
    p += dirLen;

    if (!hasTrailingSlash) {
        if (p + 1 > ep) {
            return NULL;
        }

        *p++ = '/';
    }


    // Generate a unique name
    if (p + NUM_RND_CHARS > ep) {
        return NULL;
    }

    unsigned int seed = time(NULL);
    char* sp = p;

    for (int i = 0; i < TMP_MAX; i++) {
        p = sp;

        // Generate a random sequence of letter and digits as the actual file name
        __generate_rnd_chars(p, seed);
        p += NUM_RND_CHARS;


        // Trailing NUL (we've reserved space for this initially)
        *p = '\0';

        if (pOutIoc) {
            const errno_t err = mkfile(filename, O_RDWR | O_EXCL, FilePermissions_MakeFromOctal(0600), pOutIoc);
            if (err == EOK) {
                return filename;
            }
        }
        else {
            if (access(filename, F_OK) != 0 && errno == ENOENT) {
                return filename;
            }
        }
    }

    return NULL;
}

char *tmpnam_r(char *filename)
{
    return __tmpnam_r(filename, NULL);
}

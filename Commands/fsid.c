//
//  fsid.c
//  cmds
//
//  Created by Dietmar Planitzer on 4/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <System/File.h>
#include <System/Process.h>


int main(int argc, char* argv[])
{
    FileInfo info;
    char buf[12];
    errno_t err = 0;
    char* path = NULL;
    bool doFree = false;

    if (argc < 2 ) {
        path = malloc(PATH_MAX);
        if (path) {
            err = Process_GetWorkingDirectory(path, PATH_MAX);
            doFree = true;
        }
        else {
            err = ENOMEM;
        }
    }
    else {
        path = argv[1];
    }


    if (err == 0) {
        err = File_GetInfo(path, &info);
        if (err == 0) {
            printf("%u\n", info.fsid);
        }
    }


    if (doFree) {
        free(path);
    }

    return (err == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

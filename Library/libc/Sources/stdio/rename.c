//
//  rename.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <System/System.h>


int rename(const char* oldpath, const char* newpath)
{
    decl_try_err();

    err = File_Rename(oldpath, newpath);
    if (err != 0) {
        errno = err;
        return -1;
    }
    else {
        return 0;
    }
}

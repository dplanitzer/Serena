//
//  remove.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <System/System.h>


int remove(const char* path)
{
    decl_try_err();

    err = File_Unlink(path);
    if (err != 0) {
        errno = err;
        return -1;
    }
    else {
        return 0;
    }
}

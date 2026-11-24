//
//  hd.c
//  diskimage
//
//  Created by Dietmar Planitzer on 11/14/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "hd_path.h"
#include <string.h>

errno_t hd_validate_path(const char* path)
{
    const size_t len = strlen(path);

    if (len == 0) {
        return EINVAL;
    }
    if (len > HD_PATH_MAX-1) {
        return ENAMETOOLONG;
    }

    return 0;
}

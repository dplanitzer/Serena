//
//  dd.h
//  diskimage
//
//  Created by Dietmar Planitzer on 11/24/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef DD_H
#define DD_H

#include <stdio.h>
#include <dd/dd_path.h>
#include <dd/dd_rdir.h>
#include "FSManager.h"

/* What a path refers to */
enum {
    DD_KIND_ERROR = -1,     /* it couldn't be determined; errno is set */
    DD_KIND_NONE = 0,       /* nothing */
    DD_KIND_DIRECTORY = 1,
    DD_KIND_OTHER = 2       /* a file or anything else that isn't a directory */
};


extern int dd_path_kind(FSManagerRef fsm, const char *path);

#endif /* DD_H */

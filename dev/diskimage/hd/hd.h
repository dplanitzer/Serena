//
//  hd_path.h
//  diskimage
//
//  Created by Dietmar Planitzer on 11/14/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef HD_H
#define HD_H

#include <stdio.h>
#include <hd/hd_path.h>
#include <hd/hd_rdir.h>

/* What a path refers to */
enum {
    HD_KIND_ERROR = -1,     /* it couldn't be determined; errno is set */
    HD_KIND_NONE = 0,       /* nothing */
    HD_KIND_DIRECTORY = 1,
    HD_KIND_OTHER = 2       /* a file or anything else that isn't a directory */
};


extern int hd_path_kind(const char *path);
extern int hd_create_dir(const char *path, int *created);
extern FILE* hd_fopen(const char *path, const char* mode);

#endif /* HD_H */

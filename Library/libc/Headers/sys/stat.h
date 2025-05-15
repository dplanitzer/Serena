//
//  sys/stat.h
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_STAT_H
#define _SYS_STAT_H 1

#include <sys/types.h>
#include <System/FilePermissions.h>
#include <System/File.h>

__CPP_BEGIN

// Creates an empty directory with the name and at the filesystem location specified
// by 'path'. 'mode' specifies the permissions that should be assigned to the
// directory.
// @Concurrency: Safe
extern int mkdir(const char* _Nonnull path, FilePermissions mode);

__CPP_END

#endif /* _SYS_STAT_H */

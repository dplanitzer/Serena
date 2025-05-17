//
//  sys/stat.h
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_STAT_H
#define _SYS_STAT_H 1

#include <_cmndef.h>
#include <kpi/stat.h>
#include <sys/errno.h>

__CPP_BEGIN

// Creates an empty directory with the name and at the filesystem location specified
// by 'path'. 'mode' specifies the permissions that should be assigned to the
// directory.
// @Concurrency: Safe
extern int mkdir(const char* _Nonnull path, FilePermissions mode);

// Returns meta-information about the file located at the filesystem location 'path'.
// @Concurrency: Safe
extern errno_t getfinfo(const char* _Nonnull path, finfo_t* _Nonnull info);

// Updates the meta-information about the file located at the filesystem location
// 'path'. Note that only those pieces of the meta-information are modified for
// which the corresponding flag in 'info.modify' is set.
// @Concurrency: Safe
extern errno_t setfinfo(const char* _Nonnull path, fmutinfo_t* _Nonnull info);



// Similar to getfinfo() but operates on the open file identified by 'ioc'.
// @Concurrency: Safe
extern errno_t fgetfinfo(int ioc, finfo_t* _Nonnull info);

// Similar to setfinfo() but operates on the open file identified by 'ioc'.
// @Concurrency: Safe
extern errno_t fsetfinfo(int ioc, fmutinfo_t* _Nonnull info);

__CPP_END

#endif /* _SYS_STAT_H */

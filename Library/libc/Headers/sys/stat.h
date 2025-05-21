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
#include <kpi/_time.h>

__CPP_BEGIN

// Creates an empty directory with the name and at the filesystem location specified
// by 'path'. 'mode' specifies the permissions that should be assigned to the
// directory.
// @Concurrency: Safe
extern int mkdir(const char* _Nonnull path, mode_t mode);


// Returns meta-information about the file located at the filesystem location 'path'.
// @Concurrency: Safe
extern int stat(const char* _Nonnull path, struct stat* _Nonnull info);

// Similar to stat() but operates on the open file identified by 'ioc'.
// @Concurrency: Safe
extern int fstat(int ioc, struct stat* _Nonnull info);


// Changes the file permission bits of the file or directory at 'path' to the
// file permissions encoded in 'mode'.
// @Concurrency: Safe
extern int chmod(const char* _Nonnull path, mode_t mode);

// Sets the access and modification date of the file at 'path'. The dates are
// set to the current time if 'times' is NULL.
// @Concurrency: Safe
extern int utimens(const char* _Nonnull path, const struct timespec times[_Nullable 2]);


// Sets the process' umask. Bits set in this mask are cleared in the permissions
// that are used to create a file. Returns the old umask. Note that calling this
// function with SEO_UMASK_NO_CHANGE as the argument causes umask() to simply
// return the current umask without chaning it as a side-effect.
// @Concurrency: Safe
extern mode_t umask(mode_t mask);

__CPP_END

#endif /* _SYS_STAT_H */

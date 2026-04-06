//
//  file.h
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SERENA_FILE_H
#define _SERENA_FILE_H 1

#include <kpi/_access.h>
#include <kpi/file.h>
#include <serena/fd.h>

__CPP_BEGIN

// Same as open() with O_CREAT|O_WRONLY|O_TRUNC.
// @Concurrency: Safe
extern int creat(const char* _Nonnull path, mode_t mode);


// Returns meta-information about the file located at the filesystem location 'path'.
// @Concurrency: Safe
extern int stat(const char* _Nonnull path, struct stat* _Nonnull info);

// Similar to stat() but operates on the open file identified by 'ioc'.
// @Concurrency: Safe
extern int fstat(int ioc, struct stat* _Nonnull info);


// Checks whether the file at the filesystem location 'path' exists and whether
// it is accessible according to 'mode'. A suitable error is returned otherwise.
// @Concurrency: Safe
extern int access(const char* _Nonnull path, int mode);

// Truncates the file at the filesystem location 'path'. If the new length is
// greater than the size of the existing file, then the file is expanded and the
// newly added data range is zero-filled. If the new length is less than the
// size of the existing file, then the excess data is removed and the size of
// the file is set to the new length.
// @Concurrency: Safe
extern int truncate(const char* _Nonnull path, off_t length);

// Similar to truncate() but operates on the open file identified by 'ioc'.
// @Concurrency: Safe
extern int ftruncate(int fd, off_t length);


// Deletes the file located at the filesystem location 'path'.
// @Concurrency: Safe
extern int unlink(const char* _Nonnull path);


// Changes the file permission bits of the file or directory at 'path' to the
// file permissions encoded in 'mode'.
// @Concurrency: Safe
extern int chmod(const char* _Nonnull path, mode_t mode);

extern int chown(const char* _Nonnull path, uid_t uid, gid_t gid);


// Sets the access and modification date of the file at 'path'. The dates are
// set to the current time if 'times' is NULL.
// @Concurrency: Safe
extern int utimens(const char* _Nonnull path, const struct timespec times[_Nullable 2]);

struct utimbuf {
    time_t  actime;
    time_t  modtime;
};

extern int utime(const char* _Nonnull path, const struct utimbuf* _Nullable times);

__CPP_END

#endif /* _SERENA_FILE_H */

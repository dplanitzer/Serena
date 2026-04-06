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

// Similar to stat() but operates on the open file identified by 'fd'.
// @Concurrency: Safe
extern int fstat(int fd, struct stat* _Nonnull info);


// Checks whether the file at the filesystem location 'path' exists and whether
// it is accessible according to 'mode'. A suitable error is returned otherwise.
// @Concurrency: Safe
extern int fs_access(dir_t* _Nullable wd, const char* _Nonnull path, int mode);

// Truncates the file at the filesystem location 'path'. If the new length is
// greater than the size of the existing file, then the file is expanded and the
// newly added data range is zero-filled. If the new length is less than the
// size of the existing file, then the excess data is removed and the size of
// the file is set to the new length.
// @Concurrency: Safe
extern int fs_truncate(dir_t* _Nullable wd, const char* _Nonnull path, off_t length);

// Similar to fs_truncate() but operates on the open file identified by 'fd'.
// @Concurrency: Safe
extern int ftruncate(int fd, off_t length);


// Removes the directory entry at 'path' from its containing directory. The
// object referenced by this directory entry is only deleted from the filesystem
// if this was the last directory entry referencing it. Note that filesystem
// objects are reference counted.
// If the object is a directory, then the directory must be empty before it can
// be deleted.
// @Concurrency: Safe
extern int fs_remove(dir_t* _Nullable wd, const char* _Nonnull path);


// Changes the file permission bits of the file or directory at 'path' to the
// file permissions encoded in 'mode'.
// @Concurrency: Safe
extern int fs_setperms(dir_t* _Nullable wd, const char* _Nonnull path, mode_t mode);

extern int fs_setowner(dir_t* _Nullable wd, const char* _Nonnull path, uid_t uid, gid_t gid);


// Sets the access and modification date of the file at 'path'. The dates are
// set to the current time if 'times' is NULL.
// @Concurrency: Safe
extern int fs_settimes(dir_t* _Nullable wd, const char* _Nonnull path, const struct timespec times[_Nullable 2]);

__CPP_END

#endif /* _SERENA_FILE_H */

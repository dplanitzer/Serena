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
#include <kpi/attr.h>
#include <kpi/file.h>
#include <serena/fd.h>

__CPP_BEGIN

// Opens an already existing file located at the filesystem location 'path'.
// Returns an error if the file does not exist or the caller lacks the necessary
// permissions to successfully open the file. 'oflags' specifies whether the file
// should be opened for reading and/or writing. 'O_APPEND' may be passed in
// addition to 'O_WRONLY' to force the system to always append any newly written
// data to the file. The file position is disregarded by the write function(s) in
// this case.
// @Concurrency: Safe
extern int fs_open(const char* _Nonnull path, int oflags);

// Creates an empty regular file if it doesn't already exists or truncates the
// contents of a file if it already exists. If the file already exists and
// O_EXCL is specified then the call fails instead of truncating the file.
// Either way, the file is opened for reading and/or writing after it has been
// created.
// @Concurrency: Safe
extern int fs_create_file(const char* _Nonnull path, int oflags, fs_perms_t fsperms);

// Returns the attributes of the filesystem object located at 'path'.
// @Concurrency: Safe
extern int fs_attr(dir_t* _Nullable wd, const char* _Nonnull path, fs_attr_t* _Nonnull attr);


// Changes the file permission bits of the file or directory at 'path' to the
// file permissions encoded in 'fsperms'.
// @Concurrency: Safe
extern int fs_setperms(dir_t* _Nullable wd, const char* _Nonnull path, fs_perms_t fsperms);

extern int fs_setowner(dir_t* _Nullable wd, const char* _Nonnull path, uid_t uid, gid_t gid);


// Sets the access and modification date of the file at 'path'. The dates are
// set to the current time if 'times' is NULL.
// @Concurrency: Safe
extern int fs_settimes(dir_t* _Nullable wd, const char* _Nonnull path, const nanotime_t times[_Nullable 2]);


// Truncates the file at the filesystem location 'path'. If the new length is
// greater than the size of the existing file, then the file is expanded and the
// newly added data range is zero-filled. If the new length is less than the
// size of the existing file, then the excess data is removed and the size of
// the file is set to the new length.
// @Concurrency: Safe
extern int fs_truncate(dir_t* _Nullable wd, const char* _Nonnull path, off_t length);


// Removes the directory entry at 'path' from its containing directory. The
// object referenced by this directory entry is only deleted from the filesystem
// if this was the last directory entry referencing it. Note that filesystem
// objects are reference counted.
// If the object is a directory, then the directory must be empty before it can
// be deleted.
// @Concurrency: Safe
extern int fs_remove(dir_t* _Nullable wd, const char* _Nonnull path);

// @Concurrency: Safe
extern int fs_rename(dir_t* _Nullable owd, const char* _Nonnull oldpath, dir_t* _Nullable nwd, const char* _Nonnull newpath);

// Checks whether the file at the filesystem location 'path' exists and whether
// it is accessible according to 'mode'. A suitable error is returned otherwise.
// @Concurrency: Safe
extern int fs_access(dir_t* _Nullable wd, const char* _Nonnull path, int mode);

__CPP_END

#endif /* _SERENA_FILE_H */

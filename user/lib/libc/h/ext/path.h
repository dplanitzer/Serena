//
//  ext/path.h
//  libc
//
//  Created by Dietmar Planitzer on 10/24/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _EXT_PATH_H
#define _EXT_PATH_H

#include <_cmndef.h>
#include <_ssize.h>
#include <_size.h>


#define PATH_SEPARATOR  '/'


__CPP_BEGIN

// Constructs a path from the given directory, stem and extension components.
// The resulting path is stored in the buffer 'buf' which has sized 'buf_size'
// bytes. This size includes the space that is needed for the trailing '\0'
// character. The function returns 0 on success and -1 on an error. An EINVAL
// error is returned if incorrect parameters are passed to this function. I.e.
// 'buf_size' has to be at least 1 if 'buf' is not NULL. An ENAMETOOLONG error
// is returned if the resulting path does not fit inside the provided buffer.
// Calling this function with 'buf' equal NULL causes it to simply calculate
// the size of the resulting path and this size is returned as the function
// result.
extern ssize_t makepath_r(char* _Nullable buf, size_t buf_size, const char* _Nullable dir, const char* _Nullable stem, const char* _Nullable ext);


// Converts the provided relative path 'rel_path' to an absolute path and stores
// it in 'buf'. 'buf_size' is the size of 'buf' including the trailing '\0'
// character. If 'rel_path' is actually absolute, then 'rel_path' is simply
// copied to 'buf' as is. Otherwise 'rel_path' is prefixed with the path to the
// process' current working directory and the result is stored in 'buf'. A
// pointer to 'buf' is returned on success and NULL is returned and errno is set
// to ENAMETOOLONG if 'buf' is too small to hold the full path. 
extern char* _Nullable fullpath_r(char* _Nonnull buf, size_t buf_size, const char* _Nullable rel_path);


// Copies the path 'path' into the path buffer 'buf' with size 'buf_size'. The
// 'buf_size' includes the trailing '\0' character. The function returns 0 on
// success and -1 on failure. It sets errno to ENAMETOOLONG if the provided path
// does not fit in the destination buffer. Use this function to copy a path while
// making sure that the path to copy isn't larger than expected.
extern int pathcopy_r(char* _Nonnull buf, size_t buf_size, const char* _Nonnull path);


// Appends the path component 'component' to the path stored in the buffer 'buf'
// and returns 0 on success. 'buf_size' is the size of the path buffer 'buf'.
// Note that this size includes space for the trailing '\0' character. If 'buf'
// is NULL then this function calculates the size of the resulting path including
// the trailing '\0' and it returns this size. -1 is returned and errno is set to
// ENAMETOOLONG if the resulting path does not fit in the provided buffer.
extern ssize_t pathjoin_r(char* _Nullable buf, size_t buf_size, const char* _Nonnull component);


// Extracts the directory sub-path of the path 'path'. The result is written to
// the buffer 'buf' of size 'buf_size'. The buffer size includes the space for
// the required trailing '\0' character. If 'buf' is NULL then the function
// returns the required buffer size. The function returns "." if path is the
// empty string.
extern ssize_t dirname_r(char* _Nullable buf, size_t buf_size, const char* _Nonnull path);


// Extracts the last path component of the path 'path', stores it in 'buf' and.
// returns 0 on success. 'buf' is of size 'buf_size'. This size includes space
// for the trailing '\0' character. If 'buf' is NULL then this function calculates
// the space needed to store the last path component and returns this number as
// the function result. -1 is returned and errno is set to ENAMETOOLONG if 'buf'
// is not big enough to hold the last path component. Note that this function
// returns a last path component of "." if 'path' is the empty string.
extern ssize_t basename_r(char* _Nullable buf, size_t buf_size, const char* _Nonnull path);


// Similar to basename_r() but returns the stem of the file name only. The stem
// is the filename minus the extension.
extern ssize_t stemname_r(char* _Nullable buf, size_t buf_size, const char* _Nonnull path);


// Similar to basename_r() but returns the extension of the file name only. Returns
// an empty string if the file name has no extension attached to it. The extension
// of a file name starts with the last "." in the file name. Note that there has
// to be at least one non-slash character in front and after the dot for it to
// count as a extension separator.
extern ssize_t extname_r(char* _Nullable buf, size_t buf_size, const char* _Nonnull path);


// Returns true if the provided path is a full (absolute) path and false otherwise.
#define isfullpath(__path) \
(*(__path) == PATH_SEPARATOR)

__CPP_END

#endif /* _EXT_PATH_H */

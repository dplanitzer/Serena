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

// Calculates the size of the directory, stem and extension components of the
// path 'path'. This size includes an extra byte for the trailing '\0' that
// splitpath_r() writes for each extracted component. Thus the results of this
// function can be used to correctly and optimally size the result buffers that
// are needed for splitpath_r().
// The 'dir_len', 'stem_len' and 'ext_len' pointers may be NULL. A NULL entry is
// ignored. Note that the stem plus extension equals the file name of a path.
extern void sizepath_r(const char* _Nonnull path, size_t* _Nullable dir_len, size_t* _Nullable stem_len, size_t* _Nullable ext_len);


// Splits the path 'path' into its directory, stem and extension components. The
// directory component is the path prefix that includes all characters up until
// and including the last occurrence of the path separator. The stem part is the
// portion of the path the follows the last occurrence of the path separator and
// ends with teh last character before the dot that marks the beginning of an
// extension. Finally the extension are all trailing characters that start with
// the first character after the last dot in the path. Note that there has to be
// at least one character before this dot that isn't the path separator for it
// to count as the beginning of an extension. The stem plus the extension is what
// is known as a file name.
// The 'dir', 'stem' and 'ext' pointers may be NULL. A component is only returned
// if the corresponding buffer pointer is not NULL and the corresponding 'len'
// parameter is greater 0. The 'len' parameters define the size of the buffer.
// Note that this size includes the trailing zero byte that is appended by
// splitpath_r(). 
extern void splitpath_r(const char* _Nonnull path, char* _Nullable dir, size_t dir_len, char* _Nullable stem, size_t stem_len, char* _Nullable ext, size_t ext_len);


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

__CPP_END

#endif /* _EXT_PATH_H */

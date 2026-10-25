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


// Constructs a path from the given directory, stem and extension components and
// returns the true size of the full path. 'path_size' is the size of the
// 'path' buffer including the trailing '\0' character. This function calculate
// the true size of the required path buffer. You can check whether your buffer
// is big enough to hold the full path or whether the produced path was truncated
// by checking like this:
// 
// const ssize_t r = makepath_r(buf, buf_size, ...);
// if (r >= buf_size) {
//    // path was truncated
// }
//
// The various components are in principal optional. However some combinations
// are not permissible. I.e. a missing stem and an extension is not allowed. The
// function retuns -1 and sets errno to an appropriate error for non-permissible
// inputs.
extern ssize_t makepath_r(char* _Nonnull path, size_t path_size, const char* _Nullable dir, const char* _Nullable stem, const char* _Nullable ext);

#endif /* _EXT_PATH_H */

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


#define PATH_SEPARATOR  '/'

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

#endif /* _EXT_PATH_H */

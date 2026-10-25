//
//  ext/string.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 12/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _EXT_STRING_H
#define _EXT_STRING_H 1

#ifndef __STDC_WANT_LIB_EXT1__
#define __STDC_WANT_LIB_EXT1__ 1
#endif
#include <string.h>
#undef __STDC_WANT_LIB_EXT1__

__CPP_BEGIN

// These functions are similar to strcpy() and strcat() expect that they return
// a pointer to the trailing '\0' in the destination buffer. Thus these functions
// can be used to easily and efficiently chain multiple string concatenations.
extern char* _Nonnull strcpy_x(char* _Nonnull _Restrict dst, const char* _Nonnull _Restrict src);
extern char * _Nonnull strcat_x(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src);

// These functions are similar to strncpy() and strncat() except that they return
// a pointer to teh trailing '\0' in the destination buffer and they append at
// most 1 '\0' character to the destination buffer.
extern char * _Nonnull strncpy_x(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src, size_t max_chars);
extern char * _Nonnull strncat_x(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src, size_t max_chars);

__CPP_END

#endif /* _EXT_STRING_H */

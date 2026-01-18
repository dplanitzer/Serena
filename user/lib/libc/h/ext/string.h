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

// Like strcpy()/strncpy() except that these functions here return a pointer
// that points to the trailing '\0' in the destination buffer  
extern char* _Nonnull strcpy_x(char* _Nonnull _Restrict dst, const char* _Nonnull _Restrict src);
extern char * _Nonnull strcat_x(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src);

__CPP_END

#endif /* _EXT_STRING_H */

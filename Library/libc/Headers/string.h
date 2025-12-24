//
//  string.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STRING_H
#define _STRING_H 1

#include <_cmndef.h>
#include <_null.h>
#include <_size.h>

__CPP_BEGIN

extern size_t strlen(const char * _Nonnull str);

extern char * _Nonnull strcpy(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src);
extern char * _Nonnull strncpy(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src, size_t count);

extern char * _Nonnull strcat(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src);
extern char * _Nonnull strncat(char * _Restrict dst, const char * _Restrict src, size_t count);

extern int strcmp(const char * _Nonnull lhs, const char * _Nonnull rhs);
extern int strncmp(const char * _Nonnull lhs, const char * _Nonnull rhs, size_t count);

extern char * _Nullable strchr(const char * _Nonnull str, int ch);
extern char * _Nullable strrchr(const char * _Nonnull str, int ch);

extern char * _Nullable strstr(const char * _Nonnull str, const char * _Nonnull sub_str);

#if ___STDC_HOSTED__ == 1

extern size_t strspn(const char * _Nonnull dst, const char * _Nonnull src);
extern size_t strcspn(const char * _Nonnull dst, const char * _Nonnull src);

extern char * _Nonnull strpbrk(const char * _Nonnull dst, const char * _Nonnull break_set);
extern char * _Nullable strtok(char * _Nonnull _Restrict str, const char * _Nonnull _Restrict delim);

extern char * _Nullable strdup(const char * _Nonnull src);
extern char * _Nullable strndup(const char * _Nonnull src, size_t size);


extern char * _Nonnull strerror(int err_no);

#endif


extern void *memchr(const void * _Nonnull ptr, int c, size_t count);
extern int memcmp(const void * _Nonnull lhs, const void * _Nonnull rhs, size_t count);
extern void * _Nonnull memset(void * _Nonnull dst, int c, size_t count);

extern void * _Nonnull memcpy(void * _Nonnull _Restrict dst, const void * _Nonnull _Restrict src, size_t count);
extern void * _Nonnull memmove(void * _Nonnull _Restrict dst, const void * _Nonnull _Restrict src, size_t count);

__CPP_END

#endif /* _STRING_H */


// The following definitions depend on a switch that the includer sets before
// including this file.

__CPP_BEGIN

#if __STDC_WANT_LIB_EXT1__ == 1
extern size_t strnlen_s(const char * _Nullable str, size_t maxlen);
#endif

__CPP_END

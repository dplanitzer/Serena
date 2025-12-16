//
//  kern/string.h
//  diskimage
//
//  Created by Dietmar Planitzer on 5/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_STRING_H
#define _KERN_STRING_H 1

#include <_cmndef.h>
#include <stdbool.h>
#include <sys/types.h>

__CPP_BEGIN

extern ssize_t strlen(const char* _Nonnull pStr);
extern ssize_t strnlen(const char* _Nonnull pStr, ssize_t strsz);

extern char* _Nonnull strcpy(char* _Nonnull pDst, const char* _Nonnull pSrc);
extern char* _Nonnull strncpy(char* _Nonnull pDst, const char* _Nonnull pSrc, ssize_t count);

extern bool strcmp(const char* _Nonnull pLhs, const char* _Nonnull pRhs);
extern bool strncmp(const char* _Nonnull pLhs, const char* _Nonnull pRhs, ssize_t count);

__CPP_END

#endif /* _KERN_STRING_H */

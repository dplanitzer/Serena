//
//  string.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STRING_H
#define _STRING_H 1

#include <System/_cmndef.h>
#include <System/_nulldef.h>
#include <System/abi/_sizedef.h>

__CPP_BEGIN

extern size_t strlen(const char *str);

extern char *strcpy(char *dst, const char *src);
extern char *strncpy(char *dst, const char *src, size_t count);

extern char *strcat(char *dst, const char *src);
extern char *strncat(char *dst, const char *src, size_t count);

extern int strcmp(const char *lhs, const char *rhs);
extern int strncmp(const char *lhs, const char *rhs, size_t count);

extern char *strchr(const char *str, int ch);
extern char *strrchr(const char *str, int ch);

extern char *strstr(const char *str, const char *sub_str);

extern size_t strspn(const char *dst, const char *src);
extern size_t strcspn(const char *dst, const char *src);

extern char *strpbrk(const char *dst, const char *break_set);
extern char *strtok(char *str, const char *delim);

extern char *strdup(const char *src);
extern char *strndup(const char *src, size_t size);


extern char *strerror(int err_no);


extern void *memchr(const void *ptr, int ch, size_t count);
extern int memcmp(const void *lhs, const void *rhs, size_t count);
extern void *memset(void *dst, int ch, size_t count);
extern void *memcpy(void *dst, const void *src, size_t count);
extern void *memmove(void *dst, const void *src, size_t count);

__CPP_END

#endif /* _STRING_H */

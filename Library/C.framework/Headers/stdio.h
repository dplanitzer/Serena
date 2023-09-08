//
//  stdio.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDIO_H
#define _STDIO_H 1

#include <_cmndef.h>
#include <_nulldef.h>
#include <_sizedef.h>
#include <stdarg.h>

__CPP_BEGIN

#define EOF -1

typedef long long fpos_t;


extern int getchar(void);
extern char *gets(char *str);

extern int putchar(int ch);
extern int puts(const char *str);

extern int printf(const char *format, ...);
extern int vprintf(const char *format, va_list ap);

extern int sprintf(char *buffer, const char *format, ...);
extern int vsprintf(char *buffer, const char *format, va_list ap);
extern int snprintf(char *buffer, size_t bufsiz, const char *format, ...);
extern int vsnprintf(char *buffer, size_t bufsiz, const char *format, va_list ap);

extern int asprintf(char **str_ptr, const char *format, ...);
extern int vasprintf(char **str_ptr, const char *format, va_list ap);

extern void perror(const char *str);

__CPP_END

#endif /* _STDIO_H */

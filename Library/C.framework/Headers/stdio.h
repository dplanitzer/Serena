//
//  stdio.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDIO_H
#define _STDIO_H 1

#include <stdarg.h>
#include <stddef.h>

#define EOF -1

extern int putchar(int ch);
extern int puts(const char *str);

extern int printf(const char *format, ...);
extern int vprintf(const char *format, va_list ap);

extern int sprintf(char *buffer, const char *format, ...);
extern int vsprintf(char *buffer, const char *format, va_list ap);
extern int snprintf(char *buffer, size_t bufsiz, const char *format, ...);
extern int vsnprintf(char *buffer, size_t bufsiz, const char *format, va_list ap);

#endif /* _STDIO_H */

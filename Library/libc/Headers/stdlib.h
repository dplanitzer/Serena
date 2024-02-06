//
//  stdlib.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDLIB_H
#define _STDLIB_H 1

#include <apollo/_cmndef.h>
#include <abi/_nulldef.h>
#include <apollo/_sizedef.h>
#include <malloc.h>
#include <stdnoreturn.h>

__CPP_BEGIN

extern _Noreturn abort(void);


#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

extern int atexit(void (*func)(void));
extern _Noreturn exit(int exit_code);
extern _Noreturn _Exit(int exit_code);


extern int atoi(const char *str);
extern long atol(const char *str);
extern long long atoll(const char *str);

extern long strtol(const char *str, char **str_end, int base);
extern long long strtoll(const char *str, char **str_end, int base);

extern unsigned long strtoul(const char *str, char **str_end, int base);
extern unsigned long long strtoull(const char *str, char **str_end, int base);

extern char *itoa(int val, char *buf, int radix);
extern char *ltoa(long val, char *buf, int radix);
extern char *lltoa(long long val, char *buf, int radix);


extern int abs(int n);
extern long labs(long n);
extern long long llabs(long long n);


extern void* bsearch(const void *key, const void *ptr, size_t count, size_t size,
               int (*comp)(const void*, const void*));


extern char *getenv(const char *name);
extern int setenv(const char *name, const char *value, int overwrite);
extern int unsetenv(const char *name);

// These enviornment related APIs are broken by design. Their use is strongly
// discouraged and code which uses these functions should be updated to use the
// environment related functions listed above.
extern char **environ;
extern int putenv(char *str);

__CPP_END

#endif /* _STDLIB_H */

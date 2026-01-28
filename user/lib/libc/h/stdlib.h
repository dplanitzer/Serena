//
//  stdlib.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDLIB_H
#define _STDLIB_H 1

#if ___STDC_HOSTED__ != 1
#error "not supported in freestanding mode"
#endif

#include <_cmndef.h>
#include <_absdiv.h>
#include <_sortsearch.h>
#include <arch/_null.h>
#include <arch/_size.h>
#include <malloc.h>
#include <stdnoreturn.h>

__CPP_BEGIN

extern _Noreturn void abort(void);


#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

extern int atexit(void (*func)(void));
extern _Noreturn void exit(int status);
extern _Noreturn void _Exit(int status);


extern int atoi(const char *str);
extern long atol(const char *str);
extern long long atoll(const char *str);

extern long strtol(const char * _Restrict str, char ** _Restrict str_end, int base);
extern long long strtoll(const char * _Restrict str, char ** _Restrict str_end, int base);

extern unsigned long strtoul(const char * _Restrict str, char ** _Restrict str_end, int base);
extern unsigned long long strtoull(const char * _Restrict str, char ** _Restrict str_end, int base);


#define RAND_MAX 0x7fffffff

extern void srand(unsigned int seed);
extern int rand(void);
extern int rand_r(unsigned int *seed);


extern char *getenv(const char *name);
extern int setenv(const char *name, const char *value, int overwrite);
extern int unsetenv(const char *name);

// These enviornment related APIs are broken by design. Their use is strongly
// discouraged and code which uses these functions should be updated to use the
// environment related functions listed above.
extern char **environ;
extern int putenv(char *str);

extern int system(const char *string);

__CPP_END

#endif /* _STDLIB_H */


// The following definitions depend on a switch that the includer sets before
// including this file.

__CPP_BEGIN

#if defined(_OPEN_SYS_ITOA_EXT)
// Valid values for 'radix' are: 2, 8, 10, 16
extern char *itoa(int val, char *buf, int radix);
extern char *ltoa(long val, char *buf, int radix);
extern char *lltoa(long long val, char *buf, int radix);

extern char *utoa(unsigned int val, char *buf, int radix);
extern char *ultoa(unsigned long val, char *buf, int radix);
extern char *ulltoa(unsigned long long val, char *buf, int radix);
#endif

__CPP_END

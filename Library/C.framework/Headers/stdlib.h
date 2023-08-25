//
//  stdlib.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDLIB_H
#define _STDLIB_H 1

#include <stddef.h>
#include <stdnoreturn.h>

extern _Noreturn abort(void);


#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

extern int atexit(void (*func)(void));
extern _Noreturn exit(int exit_code);
extern _Noreturn _Exit(int exit_code);


extern int atoi(const char *str);
extern long atol(const char *str);


extern void *malloc(size_t size);
extern void *calloc(size_t num, size_t size);
extern void *realloc(void *ptr, size_t new_size);
extern void free(void *ptr);

#endif /* _STDLIB_H */

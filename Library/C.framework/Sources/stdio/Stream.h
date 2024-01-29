//
//  Stream.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Stream_h
#define Stream_h 1

#include <stdio.h>
#include <errno.h>
#include <__stddef.h>

__CPP_BEGIN

extern errno_t __fopen_init(FILE* self, __FILE_read readfn, __FILE_write writefn, __FILE_seek seekfn, __FILE_close closefn, __FILE_Mode mode);
extern errno_t __fdopen_init(FILE* self, int ioc, const char *mode);
extern FILE *__fopen(__FILE_read readfn, __FILE_write writefn, __FILE_seek seekfn, __FILE_close closefn, __FILE_Mode mode);

__CPP_END

#endif /* Stream_h */

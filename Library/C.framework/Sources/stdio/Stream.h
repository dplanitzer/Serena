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

typedef int __FILE_Mode;

enum {
    __kStreamMode_Read = 0x01,      // Allow reading
    __kStreamMode_Write = 0x02,     // Allow writing
    __kStreamMode_Append = 0x04,    // Append to the file
    __kStreamMode_Exclusive = 0x08  // Fail if file already exists instead of creating it
};

enum {
    __kStreamDirection_None = 0,
    __kStreamDirection_Read,
    __kStreamDirection_Write
};

extern __FILE_Mode __fopen_parse_mode(const char* _Nonnull mode);

extern errno_t __fopen_init(FILE* self, __FILE_read readfn, __FILE_write writefn, __FILE_seek seekfn, __FILE_close closefn, __FILE_Mode mode);
extern errno_t __fdopen_init(FILE* self, int ioc, const char *mode);
extern FILE *__fopen(__FILE_read readfn, __FILE_write writefn, __FILE_seek seekfn, __FILE_close closefn, __FILE_Mode mode);

__CPP_END

#endif /* Stream_h */

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
#include <stdbool.h>
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

extern errno_t __fopen_init(FILE* self, bool bFreeOnClose, intptr_t context, const FILE_Callbacks* callbacks, const char* mode);
extern errno_t __fdopen_init(FILE* self, bool bFreeOnClose, int ioc, const char *mode);
extern errno_t __fopen_filename_init(FILE *self, const char *filename, const char *mode);

extern FILE *__fopen_null(const char *mode);

extern int __fclose(FILE * _Nonnull s);

__CPP_END

#endif /* Stream_h */

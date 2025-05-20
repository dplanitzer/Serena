//
//  ArgumentVector.h
//  sh
//
//  Created by Dietmar Planitzer on 7/14/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef ArgumentVector_h
#define ArgumentVector_h

#include "Errors.h"
#include <stdlib.h>

typedef struct ArgumentVector {
    char**  argv;
    int     argvCapacity;
    int     argvCount;
    int     argc;

    char*   text;
    size_t  textCapacity;
    size_t  textCount;

    char*   argStart;
} ArgumentVector;


extern ArgumentVector* _Nonnull ArgumentVector_Create(void);
extern void ArgumentVector_Destroy(ArgumentVector* _Nullable self);

#define ArgumentVector_GetArgc(__self) (__self)->argc
#define ArgumentVector_GetArgv(__self) (__self)->argv

// Opens the argument vector stream for writing and removes all existing argument
// data.
extern void ArgumentVector_Open(ArgumentVector* _Nonnull self);

// Appends data to the current argument.
extern errno_t ArgumentVector_AppendCharacter(ArgumentVector* _Nonnull self, char ch);
extern errno_t ArgumentVector_AppendString(ArgumentVector* _Nonnull self, const char* _Nonnull str);
extern errno_t ArgumentVector_AppendBytes(ArgumentVector* _Nonnull self, const char* _Nonnull buf, size_t len);

// Marks the end of the current argument and creates a new argument.
extern errno_t ArgumentVector_EndOfArg(ArgumentVector* _Nonnull self);

// Closes the argument vector stream. You may call GetArgc() and GetArgv() after
// closing the stream.
extern errno_t ArgumentVector_Close(ArgumentVector* _Nonnull self);

extern void ArgumentVector_Print(ArgumentVector* _Nonnull self);

#endif  /* ArgumentVector_h */

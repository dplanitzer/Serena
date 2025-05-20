//
//  ArgumentVector.c
//  sh
//
//  Created by Dietmar Planitzer on 7/14/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ArgumentVector.h"
#include "Utilities.h"
#include <stdio.h>
#include <string.h>


#define INITIAL_TEXT_CAPACITY  256
#define INITIAL_ARGV_CAPACITY  8

ArgumentVector* _Nonnull ArgumentVector_Create(void)
{
    ArgumentVector* self = calloc(1, sizeof(ArgumentVector));

    self->text = malloc(INITIAL_TEXT_CAPACITY * sizeof(char));
    self->textCapacity = INITIAL_TEXT_CAPACITY;
    self->argv = malloc(INITIAL_ARGV_CAPACITY * sizeof(char*));
    self->argvCapacity = INITIAL_ARGV_CAPACITY;

    self->text[0] = '\0';
    self->argv[0] = NULL;

    return self;
}

void ArgumentVector_Destroy(ArgumentVector* _Nullable self)
{
    if (self) {
        free(self->text);
        self->text = NULL;
        self->textCapacity = 0;
        self->textCount = 0;

        free(self->argv);
        self->argv = NULL;
        self->argvCapacity = 0;
        self->argvCount = 0;

        free(self);
    }
}

// Opens the argument vector stream for writing and removes all existing argument
// data.
void ArgumentVector_Open(ArgumentVector* _Nonnull self)
{
    self->textCount = 0;
    self->argvCount = 0;
    self->argc = 0;
    self->argStart = self->text;
}

// Appends data to the current argument.
errno_t ArgumentVector_AppendCharacter(ArgumentVector* _Nonnull self, char ch)
{
    return ArgumentVector_AppendBytes(self, &ch, 1);
}

errno_t ArgumentVector_AppendString(ArgumentVector* _Nonnull self, const char* _Nonnull str)
{
    return ArgumentVector_AppendBytes(self, str, strlen(str));
}

errno_t ArgumentVector_AppendBytes(ArgumentVector* _Nonnull self, const char* _Nonnull buf, size_t len)
{
    if (len > 0) {
        if (self->textCount + len > self->textCapacity) {
            const size_t  newCapacity = __Ceil_PowerOf2(self->textCount + len, 256);
            char* newText = realloc(self->text, newCapacity * sizeof(char));

            if (newText == NULL) {
                return ENOMEM;
            }

            self->text = newText;
            self->textCapacity = newCapacity;
        }

        memcpy(&self->text[self->textCount], buf, len * sizeof(char));
        self->textCount += len;
    }

    return EOK;
}

static errno_t _ArgumentVector_AppendArgv(ArgumentVector* _Nonnull self, char* _Nullable ap)
{
    if (self->argvCount == self->argvCapacity) {
        const size_t  newCapacity = self->argvCapacity * 2;
        char** newArgv = realloc(self->argv, newCapacity * sizeof(char*));

        if (newArgv == NULL) {
            return ENOMEM;
        }

        self->argv = newArgv;
        self->argvCapacity = newCapacity;
    }

    self->argv[self->argvCount++] = ap;

    return EOK;
}

// Marks the end of the current argument and creates a new argument.
errno_t ArgumentVector_EndOfArg(ArgumentVector* _Nonnull self)
{
    decl_try_err();

    err = ArgumentVector_AppendCharacter(self, '\0');
    if (err == EOK) {
        err = _ArgumentVector_AppendArgv(self, self->argStart);
        self->argStart = &self->text[self->textCount];
        self->argc++;
    }

    return err;
}

// Closes the argument vector stream. You may call GetArgc() and GetArgv() after
// closing the stream.
errno_t ArgumentVector_Close(ArgumentVector* _Nonnull self)
{
    decl_try_err();

    if (&self->text[self->textCount] > self->argStart) {
        // End the current argument if anything has been written to it; otherwise
        // there's nothing to end and we don't want to add an empty extra arg
        err = ArgumentVector_EndOfArg(self);
    }
    if (err == EOK) {
        err = _ArgumentVector_AppendArgv(self, NULL);
    }

    return err;
}

void ArgumentVector_Print(ArgumentVector* _Nonnull self)
{
    for(int i = 0; i < ArgumentVector_GetArgc(self); i++) {
        printf("[%d]: '%s'\n", i, ArgumentVector_GetArgv(self)[i]);
    }
}

//
//  Interpreter.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void _chdir(const char* path)
{
    const errno_t err = setcwd(path);

    if (err != 0) {
        printf("Error: %s.\n", strerror(err));
    }
}

static void _remove(const char* path)
{
    const errno_t err = unlink(path);

    if (err != 0) {
        printf("Error: %s.\n", strerror(err));
    }
}

void _mkdir(const char* path)
{
    const errno_t err = mkdir(path, 0777);
    if (err != 0) {
        printf("Error: %s.\n", strerror(err));
    }
}

static int _opendir(const char* path)
{
    int fd = -1;
    const errno_t err = opendir(path, &fd);

    if (err != 0) {
        printf("Error: %s.\n", strerror(err));
    }
    return fd;
}

static size_t _read(int fd, void* buffer, size_t nbytes)
{
    ssize_t r = read(fd, buffer, nbytes);

    if (r < 0) {
        printf("Error: %s.\n", strerror(-r));
    }
    return (size_t)r;
}

static size_t _write(int fd, const void* buffer, size_t nbytes)
{
    ssize_t r = write(fd, buffer, nbytes);

    if (r < 0) {
        printf("Error: %s.\n", strerror(-r));
    }
    return (size_t)r;
}

static void _close(int fd)
{
    const errno_t err = close(fd);

    if (err != 0) {
        printf("Error: %s.\n", strerror(err));
    }
}


////////////////////////////////////////////////////////////////////////////////

errno_t Interpreter_Create(InterpreterRef _Nullable * _Nonnull pOutInterpreter)
{
    InterpreterRef self = (InterpreterRef)calloc(1, sizeof(Interpreter));

    if (self == NULL) {
        *pOutInterpreter = NULL;
        return ENOMEM;
    }

    if (StackAllocator_Create(1024, 8192, &self->allocator) != 0) {
        Interpreter_Destroy(self);
        *pOutInterpreter = NULL;
        return ENOMEM;
    }

    *pOutInterpreter = self;
    return 0;
}

void Interpreter_Destroy(InterpreterRef _Nullable self)
{
    if (self) {
        StackAllocator_Destroy(self->allocator);
        free(self);
    }
}

////////////////////////////////////////////////////////////////////////////////

static char* _Nullable Interpreter_ExpandWord(InterpreterRef _Nonnull self, WordRef _Nonnull pWord)
{
    size_t wordSize = 0;
    MorphemeRef pCurMorpheme;

    // Figure out how big the expanded word will be
    pCurMorpheme = pWord->morphemes;
    while (pCurMorpheme) {
        switch (pCurMorpheme->type) {
            case kMorpheme_UnquotedString:
            case kMorpheme_SingleQuotedString:
            case kMorpheme_DoubleQuotedString:
            case kMorpheme_EscapeSequence: {
                const StringMorpheme* mp = (StringMorpheme*)pCurMorpheme;
                wordSize += strlen(mp->string);
                break;
            }

            case kMorpheme_VariableReference:
                // XXX
                break;

            case kMorpheme_NestedBlock:
                // XXX
                break;
        }

        pCurMorpheme = pCurMorpheme->next;
    }
    wordSize++;

    char* str = (char*)StackAllocator_Alloc(self->allocator, wordSize);
    if (str == NULL) {
        printf("OOM\n");
        return NULL;
    }


    // Do the actual expansion
    *str = '\0';
    pCurMorpheme = pWord->morphemes;
    while (pCurMorpheme) {
        switch (pCurMorpheme->type) {
            case kMorpheme_UnquotedString:
            case kMorpheme_SingleQuotedString:
            case kMorpheme_DoubleQuotedString:
            case kMorpheme_EscapeSequence: {
                const StringMorpheme* mp = (StringMorpheme*)pCurMorpheme;
                strcat(str, mp->string);
                break;
            }

            case kMorpheme_VariableReference:
                // XXX
                break;

            case kMorpheme_NestedBlock:
                // XXX
                break;
        }

        pCurMorpheme = pCurMorpheme->next;
    }

    return str;
}

static char* _Nullable Interpreter_GetArgumentAt(InterpreterRef _Nonnull self, WordRef _Nullable pArgs, int idx, char* _Nullable pDefault)
{
    WordRef pCurWord = pArgs;
    int i = 0;

    while (i < idx) {
        if (pCurWord == NULL) {
            return pDefault;
        }

        pCurWord = pCurWord->next;
        i++;
    }

    char* str = Interpreter_ExpandWord(self, pCurWord);
    if (str == NULL || *str == '\0') {
        return pDefault;
    }
    return str;
}


////////////////////////////////////////////////////////////////////////////////

static void sh_cd(InterpreterRef _Nonnull self, WordRef _Nullable pArgs)
{
    char* path = Interpreter_GetArgumentAt(self, pArgs, 0, NULL);
    
    if (path == NULL) {
        printf("Error: expected a path.\n");
        return;
    }

    _chdir(path);
}

static void sh_ls(InterpreterRef _Nonnull self, WordRef _Nullable pArgs)
{
    char* path = Interpreter_GetArgumentAt(self, pArgs, 0, ".");

    const int fd = _opendir(path);
    if (fd != -1) {
        while (true) {
            struct _directory_entry_t dirent;
            const ssize_t r = _read(fd, &dirent, sizeof(dirent));
        
            if (r == 0) {
                break;
            }

            printf("%ld:\t\"%s\"\n", dirent.inodeId, dirent.name);
        }
        _close(fd);
    }
}

static void sh_pwd(InterpreterRef _Nonnull self, WordRef _Nullable pArgs)
{
    if (pArgs) {
        printf("Warning: ignored unexpected arguments\n");
    }

    char buf[128];  //XXX Should be PATH_MAX, however our user stack is not big enough to park it there
    const errno_t err = getcwd(buf, sizeof(buf));

    if (err == 0) {
        printf("%s\n", buf);
    } else {
        printf("Error: %s.\n", strerror(err));
    }
}

static void sh_mkdir(InterpreterRef _Nonnull self, WordRef _Nullable pArgs)
{
    char* path = Interpreter_GetArgumentAt(self, pArgs, 0, NULL);

    if (path == NULL) {
        printf("Error: expected a path\n");
        return;
    }

    _mkdir(path);
}

static void sh_rm(InterpreterRef _Nonnull self, WordRef _Nullable pArgs)
{
    char* path = Interpreter_GetArgumentAt(self, pArgs, 0, NULL);

    if (path == NULL) {
        printf("Error: expected a path");
        return;
    }

    _remove(path);
}


////////////////////////////////////////////////////////////////////////////////

static void Interpreter_Sentence(InterpreterRef _Nonnull self, SentenceRef _Nonnull pSentence)
{
    if (pSentence->words == NULL) {
        return;
    }

    const char* cmd = Interpreter_ExpandWord(self, pSentence->words);
    WordRef pArgs = pSentence->words->next;

    if (!strcmp(cmd, "cd")) {
        sh_cd(self, pArgs);
    }
    else if (!strcmp(cmd, "list")) {
        sh_ls(self, pArgs);
    }
    else if (!strcmp(cmd, "pwd")) {
        sh_pwd(self, pArgs);
    }
    else if (!strcmp(cmd, "makedir")) {
        sh_mkdir(self, pArgs);
    }
    else if (!strcmp(cmd, "delete")) {
        sh_rm(self, pArgs);
    }
    else {
        printf("Error: unknown command.\n");
    }
}

static void Interpreter_Block(InterpreterRef _Nonnull self, BlockRef _Nonnull pBlock)
{
    SentenceRef st = pBlock->sentences;

    while (st) {
        Interpreter_Sentence(self, st);
        st = st->next;
    }
}

// Interprets 'pScript' and executes all its statements.
void Interpreter_Execute(InterpreterRef _Nonnull self, ScriptRef _Nonnull pScript)
{
    //Script_Print(pScript);
    Interpreter_Block(self, pScript->block);
    StackAllocator_DeallocAll(self->allocator);
}

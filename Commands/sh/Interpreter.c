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
#include <abi/_math.h>


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

void _getfileinfo(const char* path, struct _file_info_t* info)
{
    const errno_t err = getfileinfo(path, info);
    if (err != 0) {
        printf("Error: %s.\n", strerror(err));
    }
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

    self->pathBuffer = (char*)malloc(PATH_MAX);
    if (self->pathBuffer == NULL) {
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
        self->allocator = NULL;
        free(self->pathBuffer);
        self->pathBuffer = NULL;
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

static void file_permissions_to_text(_file_permissions_t perms, char* _Nonnull buf)
{
    if ((perms & kFilePermission_Read) == kFilePermission_Read) {
        buf[0] = 'r';
    }
    if ((perms & kFilePermission_Write) == kFilePermission_Write) {
        buf[1] = 'w';
    }
    if ((perms & kFilePermission_Execute) == kFilePermission_Execute) {
        buf[2] = 'x';
    }
}

static errno_t calc_dir_entry_format(InterpreterRef _Nonnull self, const char* _Nonnull pDirPath, struct _directory_entry_t* _Nonnull pEntry, void* _Nullable pContext)
{
    struct DirectoryEntryFormat* fmt = (struct DirectoryEntryFormat*)pContext;
    struct _file_info_t info;

    strcpy(self->pathBuffer, pDirPath);
    strcat(self->pathBuffer, "/");
    strcat(self->pathBuffer, pEntry->name);

    _getfileinfo(self->pathBuffer, &info);

    itoa(info.linkCount, self->pathBuffer, 10);
    fmt->linkCountWidth = __max(fmt->linkCountWidth, strlen(self->pathBuffer));
    itoa(info.uid, self->pathBuffer, 10);
    fmt->uidWidth = __max(fmt->uidWidth, strlen(self->pathBuffer));
    itoa(info.gid, self->pathBuffer, 10);
    fmt->gidWidth = __max(fmt->gidWidth, strlen(self->pathBuffer));
    lltoa(info.size, self->pathBuffer, 10);
    fmt->sizeWidth = __max(fmt->sizeWidth, strlen(self->pathBuffer));
    itoa(info.inodeId, self->pathBuffer, 10);
    fmt->inodeIdWidth = __max(fmt->inodeIdWidth, strlen(self->pathBuffer));

    return 0;
}

static errno_t print_dir_entry(InterpreterRef _Nonnull self, const char* _Nonnull pDirPath, struct _directory_entry_t* _Nonnull pEntry, void* _Nullable pContext)
{
    struct DirectoryEntryFormat* fmt = (struct DirectoryEntryFormat*)pContext;
    struct _file_info_t info;

    strcpy(self->pathBuffer, pDirPath);
    strcat(self->pathBuffer, "/");
    strcat(self->pathBuffer, pEntry->name);

    _getfileinfo(self->pathBuffer, &info);

    char tp[11];
    for (int i = 0; i < sizeof(tp); i++) {
        tp[i] = '-';
    }
    if (info.type == kFileType_Directory) {
        tp[0] = 'd';
    }
    file_permissions_to_text(FilePermissions_Get(info.permissions, kFilePermissionScope_User), &tp[1]);
    file_permissions_to_text(FilePermissions_Get(info.permissions, kFilePermissionScope_Group), &tp[4]);
    file_permissions_to_text(FilePermissions_Get(info.permissions, kFilePermissionScope_Other), &tp[7]);
    tp[10] = '\0';

    printf("%s %*ld  %*lu %*lu  %*lld %*ld %s\n",
        tp,
        fmt->linkCountWidth, info.linkCount,
        fmt->uidWidth, info.uid,
        fmt->gidWidth, info.gid,
        fmt->sizeWidth, info.size,
        fmt->inodeIdWidth, info.inodeId,
        pEntry->name);
    return 0;
}

static void iterate_dir(InterpreterRef _Nonnull self, const char* _Nonnull path, DirectoryIteratorCallback _Nonnull cb, void* _Nullable pContext)
{
    const int fd = _opendir(path);
    errno_t err = 0;

    if (fd != -1) {
        while (err == 0) {
            struct _directory_entry_t dirent;
            const ssize_t r = _read(fd, &dirent, sizeof(dirent));
        
            if (r == 0) {
                break;
            }

            err = cb(self, path, &dirent, pContext);
        }
        _close(fd);
    }
}

static void sh_ls(InterpreterRef _Nonnull self, WordRef _Nullable pArgs)
{
    char* path = Interpreter_GetArgumentAt(self, pArgs, 0, ".");
    struct DirectoryEntryFormat fmt = {0};

    iterate_dir(self, path, calc_dir_entry_format, &fmt);
    iterate_dir(self, path, print_dir_entry, &fmt);
}

static void sh_pwd(InterpreterRef _Nonnull self, WordRef _Nullable pArgs)
{
    if (pArgs) {
        printf("Warning: ignored unexpected arguments\n");
    }

    const errno_t err = getcwd(self->pathBuffer, PATH_MAX);

    if (err == 0) {
        printf("%s\n", self->pathBuffer);
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

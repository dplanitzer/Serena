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

// Expands the given word to a string. Expansion means that:
// XXX
// Returns the string on success (note that it may be empty) and NULL if there's
// not enough memory to do the expansion.
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
        return NULL;
    }


    // Do the actual expansion
    str[0] = '\0';
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


////////////////////////////////////////////////////////////////////////////////

static int sh_cd(InterpreterRef _Nonnull self, int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "";
    
    if (*path == '\0') {
        printf("%s: expected a path.\n", argv[0]);
        return EXIT_FAILURE;
    }

    const errno_t err = setcwd(path);
    if (err != 0) {
        printf("%s: %s.\n", argv[0], strerror(err));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
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
    errno_t err = 0;

    strcpy(self->pathBuffer, pDirPath);
    strcat(self->pathBuffer, "/");
    strcat(self->pathBuffer, pEntry->name);

    err = getfileinfo(self->pathBuffer, &info);
    if (err != 0) {
        return err;
    }

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
    errno_t err = 0;

    strcpy(self->pathBuffer, pDirPath);
    strcat(self->pathBuffer, "/");
    strcat(self->pathBuffer, pEntry->name);

    err = getfileinfo(self->pathBuffer, &info);
    if (err != 0) {
        return err;
    }

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

static errno_t iterate_dir(InterpreterRef _Nonnull self, const char* _Nonnull path, DirectoryIteratorCallback _Nonnull cb, void* _Nullable pContext)
{
    int fd;
    errno_t err = 0;

    err = opendir(path, &fd);
    if (err == 0) {
        while (err == 0) {
            struct _directory_entry_t dirent;
            ssize_t r;

            err = read(fd, &dirent, sizeof(dirent), &r);        
            if (err != 0 || r == 0) {
                break;
            }

            err = cb(self, path, &dirent, pContext);
            if (err != 0) {
                break;
            }
        }
        close(fd);
    }
    return err;
}

static int sh_ls(InterpreterRef _Nonnull self, int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : ".";
    struct DirectoryEntryFormat fmt = {0};
    errno_t err;

    err = iterate_dir(self, path, calc_dir_entry_format, &fmt);
    if (err == 0) {
        err = iterate_dir(self, path, print_dir_entry, &fmt);
    }

    if (err != 0) {
        printf("%s: %s.\n", argv[0], strerror(err));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int sh_pwd(InterpreterRef _Nonnull self, int argc, char** argv)
{
    if (argc > 1) {
        printf("%s: unexpected extra arguments\n", argv[0]);
    }

    const errno_t err = getcwd(self->pathBuffer, PATH_MAX);
    if (err == 0) {
        printf("%s\n", self->pathBuffer);
    } else {
        printf("%s: %s.\n", argv[0], strerror(err));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int sh_mkdir(InterpreterRef _Nonnull self, int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "";

    if (*path == '\0') {
        printf("%s: expected a path.\n", argv[0]);
        return EXIT_FAILURE;
    }

    const errno_t err = mkdir(path, 0755);
    if (err != 0) {
        printf("%s: %s.\n", argv[0], strerror(err));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int sh_rm(InterpreterRef _Nonnull self, int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "";

    if (*path == '\0') {
        printf("%s: expected a path.\n", argv[0]);
        return EXIT_FAILURE;
    }

    const errno_t err = remove(path);
    if (err != 0) {
        printf("%s: %s.\n", argv[0], strerror(err));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


////////////////////////////////////////////////////////////////////////////////

static void Interpreter_Sentence(InterpreterRef _Nonnull self, SentenceRef _Nonnull pSentence)
{
    const int nWords = Sentence_GetWordCount(pSentence);

    // Create the command argument vector by expanding all words in the sentence
    // to strings.
    char** argv = (char**)StackAllocator_Alloc(self->allocator, sizeof(char*) * (nWords + 1));
    if (argv == NULL) {
        printf(strerror(ENOMEM));
        return;
    }

    int argc = 0;
    WordRef curWord = pSentence->words;
    while (curWord) {
        char* arg = Interpreter_ExpandWord(self, curWord);

        if (arg == NULL) {
            printf(strerror(ENOMEM));
            return;
        }

        argv[argc++] = arg;
        curWord = curWord->next;
    }
    argv[argc] = NULL;

    if (argc == 0) {
        return;
    }


    // Execute the command
    if (!strcmp(argv[0], "cd")) {
        sh_cd(self, argc, argv);
    }
    else if (!strcmp(argv[0], "list")) {
        sh_ls(self, argc, argv);
    }
    else if (!strcmp(argv[0], "pwd")) {
        sh_pwd(self, argc, argv);
    }
    else if (!strcmp(argv[0], "makedir")) {
        sh_mkdir(self, argc, argv);
    }
    else if (!strcmp(argv[0], "delete")) {
        sh_rm(self, argc, argv);
    }
    else {
        printf("%s: unknown command.\n", argv[0]);
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

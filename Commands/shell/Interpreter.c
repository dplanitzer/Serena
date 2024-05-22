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
#include <System/System.h>

typedef int (*InterpreterCommandCallback)(ShellContextRef _Nonnull, int argc, char** argv);

typedef struct InterpreterCommand {
    const char* _Nonnull                name;
    InterpreterCommandCallback _Nonnull cb;
} InterpreterCommand;


extern int cmd_cd(ShellContextRef _Nonnull pContext, int argc, char** argv);
extern int cmd_cls(ShellContextRef _Nonnull pContext, int argc, char** argv);
extern int cmd_delete(ShellContextRef _Nonnull pContext, int argc, char** argv);
extern int cmd_echo(ShellContextRef _Nonnull pContext, int argc, char** argv);
extern int cmd_exit(ShellContextRef _Nonnull pContext, int argc, char** argv);
extern int cmd_history(ShellContextRef _Nonnull pContext, int argc, char** argv);
extern int cmd_list(ShellContextRef _Nonnull pContext, int argc, char** argv);
extern int cmd_makedir(ShellContextRef _Nonnull pContext, int argc, char** argv);
extern int cmd_pwd(ShellContextRef _Nonnull pContext, int argc, char** argv);
extern int cmd_rename(ShellContextRef _Nonnull pContext, int argc, char** argv);
extern int cmd_type(ShellContextRef _Nonnull pContext, int argc, char** argv);


// Keep this table sorted by names, in ascending order
static const InterpreterCommand gBuiltinCommands[] = {
    {"cd", cmd_cd},
    {"cls", cmd_cls},
    {"delete", cmd_delete},
    {"echo", cmd_echo},
    {"exit", cmd_exit},
    {"history", cmd_history},
    {"list", cmd_list},
    {"makedir", cmd_makedir},
    {"pwd", cmd_pwd},
    {"rename", cmd_rename},
    {"type", cmd_type},
};


////////////////////////////////////////////////////////////////////////////////


errno_t Interpreter_Create(ShellContextRef _Nonnull pContext, InterpreterRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    InterpreterRef self;
    
    try_null(self, calloc(1, sizeof(Interpreter)), ENOMEM);
    try(StackAllocator_Create(1024, 8192, &self->allocator));
    self->context = pContext;

    *pOutSelf = self;
    return 0;

catch:
    Interpreter_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void Interpreter_Destroy(InterpreterRef _Nullable self)
{
    if (self) {
        self->context = NULL;
        StackAllocator_Destroy(self->allocator);
        self->allocator = NULL;
        free(self);
    }
}

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
            case kMorpheme_QuotedCharacter: {
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
            case kMorpheme_QuotedCharacter: {
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

static int xCompareCommandEntry(const char* _Nonnull lhs, const InterpreterCommand* _Nonnull rhs)
{
    return strcmp(lhs, rhs->name);
}

static bool Interpreter_ExecuteInternalCommand(InterpreterRef _Nonnull self, int argc, char** argv)
{
    const int nCmds = sizeof(gBuiltinCommands) / sizeof(InterpreterCommand);
    const InterpreterCommand* cmd = bsearch(argv[0], &gBuiltinCommands[0], nCmds, sizeof(InterpreterCommand), (int (*)(const void*, const void*))xCompareCommandEntry);

    if (cmd) {
        cmd->cb(self->context, argc, argv);
        return true;
    }

    return false;
}

static bool Interpreter_ExecuteExternalCommand(InterpreterRef _Nonnull self, int argc, char** argv)
{
    static const char* gSearchPath = "/System/Commands/";
    decl_try_err();
    ProcessId childPid;
    SpawnOptions opts = {0};
    
    const size_t cmdPathLen = strlen(gSearchPath) + strlen(argv[0]);
    if (cmdPathLen > PATH_MAX) {
        printf("Command line too long.\n");
        return true;
    }

    char* cmdPath = StackAllocator_Alloc(self->allocator, sizeof(char*) * (cmdPathLen + 1));
    if (cmdPath == NULL) {
        printf(strerror(ENOMEM));
        return true;
    }

    strcpy(cmdPath, gSearchPath);
    strcat(cmdPath, argv[0]);

    err = Process_Spawn(cmdPath, &opts, &childPid);
    if (err == ENOENT) {
        return false;
    }
    else if (err != EOK) {
        printf("%s: %s.\n", argv[0], strerror(err));
        return true;
    }

    ProcessTerminationStatus pts;
    Process_WaitForTerminationOfChild(childPid, &pts);
    
    return true;
}

static void Interpreter_Sentence(InterpreterRef _Nonnull self, SentenceRef _Nonnull pSentence)
{
    const int nWords = Sentence_GetWordCount(pSentence);

    // Create the command argument vector by expanding all words in the sentence
    // to strings.
    char** argv = StackAllocator_Alloc(self->allocator, sizeof(char*) * (nWords + 1));
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


    // Check whether this is a builtin command and execute it, if so
    if (Interpreter_ExecuteInternalCommand(self, argc, argv)) {
        return;
    }


    // Not a builtin command. Look for an external command
    if (Interpreter_ExecuteExternalCommand(self, argc, argv)) {
        return;
    }


    // Not a command at all
    printf("%s: unknown command.\n", argv[0]);
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

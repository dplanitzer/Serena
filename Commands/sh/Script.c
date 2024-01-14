//
//  Script.c
//  sh
//
//  Created by Dietmar Planitzer on 1/2/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Script.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Statement
////////////////////////////////////////////////////////////////////////////////

errno_t Statement_Create(StatementRef _Nullable * _Nonnull pOutStatement)
{
    StatementRef self = (StatementRef)calloc(1, sizeof(Statement));

    if (self == NULL) {
        *pOutStatement = NULL;
        return ENOMEM;
    }

    self->words = (char**)calloc(8, sizeof(char*));
    if (self->words == NULL) {
        Statement_Destroy(self);
        *pOutStatement = NULL;
        return ENOMEM;
    }
    self->wordCapacity = 8;

    *pOutStatement = self;
    return 0;
}

void Statement_Destroy(StatementRef _Nullable self)
{
    if (self) {
        if (self->words) {
            for (int i = 0; i < self->wordCount; i++) {
                free(self->words[i]);
                self->words[i] = NULL;
            }
            free(self->words);
            self->words = NULL;
        }
        self->next = NULL;

        free(self);
    }
}

void Statement_Print(StatementRef _Nonnull self)
{
    printf("{ ");
    for (int i = 0; i < self->wordCount; i++) {
        printf("\"%s\" ", self->words[i]);
    }
    printf("}");
}

errno_t Statement_AddWord(StatementRef _Nonnull self, const char* _Nonnull pWord)
{
    if (self->wordCount == self->wordCapacity) {
        int newCapacity = self->wordCapacity * 2;
        char** pNewWords = (char**)realloc(self->words, newCapacity * sizeof(char*));

        assert(pNewWords != NULL);
        self->words = pNewWords;
        self->wordCapacity = newCapacity;
    }

    self->words[self->wordCount] = strdup(pWord);
    assert(self->words[self->wordCount] != NULL);
    self->wordCount++;
    
    return 0;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Script
////////////////////////////////////////////////////////////////////////////////

errno_t Script_Create(ScriptRef _Nullable * _Nonnull pOutScript)
{
    ScriptRef self = (ScriptRef)calloc(1, sizeof(Script));

    if (self == NULL) {
        *pOutScript = NULL;
        return ENOMEM;
    }

    *pOutScript = self;
    return 0;
}

void Script_Reset(ScriptRef _Nonnull self)
{
    StatementRef st = self->statements;

    while(st) {
        StatementRef nx = st->next;
        Statement_Destroy(st);
        st = nx;
    }
    self->statements = NULL;
    self->lastStatement = NULL;
}

void Script_Destroy(ScriptRef _Nullable self)
{
    if (self) {
        Script_Reset(self);
        free(self);
    }
}

void Script_Print(ScriptRef _Nonnull self)
{
    printf("{ ");
    StatementRef st = self->statements;
    while(st) {
        Statement_Print(st);
        st = st->next;
    }
    printf("}\n");
}

void Script_AddStatement(ScriptRef _Nonnull self, StatementRef _Nonnull pStatement)
{
    if (self->lastStatement) {
        (self->lastStatement)->next = pStatement;
        self->lastStatement = pStatement;
    }
    else {
        self->statements = pStatement;
        self->lastStatement = pStatement;
    }
}

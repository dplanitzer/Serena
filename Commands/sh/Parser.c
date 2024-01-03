//
//  Parser.c
//  sh
//
//  Created by Dietmar Planitzer on 1/2/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Parser.h"
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


errno_t Script_Create(ScriptRef _Nullable * _Nonnull pOutScript)
{
    ScriptRef self = (ScriptRef)calloc(1, sizeof(Script));

    if (self == NULL) {
        return ENOMEM;
    }

    self->words = (char**)calloc(8, sizeof(char*));
    if (self->words == NULL) {
        Script_Destroy(self);
        return ENOMEM;
    }
    self->wordCapacity = 8;

    *pOutScript = self;
    return 0;
}

void Script_Destroy(ScriptRef _Nullable self)
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

        free(self);
    }
}

void Script_Print(ScriptRef _Nonnull self)
{
    printf("{ ");
    for (int i = 0; i < self->wordCount; i++) {
        printf("\"%s\" ", self->words[i]);
    }
    printf("}");
}

static void Script_Reset(ScriptRef _Nonnull self)
{
    for (int i = 0; i < self->wordCount; i++) {
        free(self->words[i]);
        self->words[i] = NULL;
    }
    self->wordCount = 0;
}

static errno_t Script_AddWord(ScriptRef _Nonnull self, const char* _Nonnull pWord)
{
    if (self->wordCount == self->wordCapacity) {
        int newCapacity = self->wordCapacity * 2;
        char** pNewWords = (char**)realloc(self->words, newCapacity);

        assert(pNewWords != NULL);
        self->words = pNewWords;
        self->wordCapacity = newCapacity;
    }

    self->words[self->wordCount] = (char*)strdup(pWord);
    assert(self->words[self->wordCount] != NULL);
    self->wordCount++;
    
    return 0;
}

////////////////////////////////////////////////////////////////////////////////


errno_t Parser_Create(ParserRef _Nullable * _Nonnull pOutParser)
{
    ParserRef self = (ParserRef)calloc(1, sizeof(Parser));

    if (self == NULL) {
        return ENOMEM;
    }

    const errno_t err = Lexer_Init(&self->lexer);
    if (err != 0) {
        Parser_Destroy(self);
        return err;
    }

    *pOutParser = self;
    return 0;
}

void Parser_Destroy(ParserRef _Nullable self)
{
    if (self) {
        Lexer_Deinit(&self->lexer);

        free(self);
    }
}

// statement: WORD+
// Expects that the current token on entry is a WORD token.
static void Parser_Statement(ParserRef _Nonnull self)
{
    while (true) {
        const Token* t = Lexer_GetToken(&self->lexer);

        if (t->id != kToken_Word) {
            break;
        }

        Script_AddWord(self->script, t->u.word.text);
        Lexer_ConsumeToken(&self->lexer);
    }
}

// script: (statement? EOS)*
static void Parser_Script(ParserRef _Nonnull self)
{
    while (true) {
        const Token* t = Lexer_GetToken(&self->lexer);
        
        switch (t->id) {
            case kToken_Eof:
                return;

            case kToken_Word:
                Parser_Statement(self);
                break;

            case kToken_Eos:
                Lexer_ConsumeToken(&self->lexer);
                break;

            default:
                printf("Error: unexpected input.\n");
                // Consume the token to try and resync
                Lexer_ConsumeToken(&self->lexer);
                break;
        }
    }
}

// Parses the text 'text' and updates the script object 'pScript' to reflect the
// result of parsing 'text'.
void Parser_Parse(ParserRef _Nonnull self, const char* _Nonnull text, ScriptRef _Nonnull pScript)
{
    Script_Reset(pScript);

    self->script = pScript;
    Lexer_SetInput(&self->lexer, text);
    
    Parser_Script(self);

    Lexer_SetInput(&self->lexer, NULL);
    self->script = NULL;
}

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


errno_t Parser_Create(ParserRef _Nullable * _Nonnull pOutParser)
{
    ParserRef self = (ParserRef)calloc(1, sizeof(Parser));

    if (self == NULL) {
        *pOutParser = NULL;
        return ENOMEM;
    }

    const errno_t err = Lexer_Init(&self->lexer);
    if (err != 0) {
        Parser_Destroy(self);
        *pOutParser = NULL;
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
    StatementRef pStatement = NULL;

    while (true) {
        const Token* t = Lexer_GetToken(&self->lexer);

        if (t->id != kToken_Word) {
            break;
        }

        if (pStatement == NULL) {
            Statement_Create(&pStatement);
        }

        Statement_AddWord(pStatement, t->u.word.text);
        Lexer_ConsumeToken(&self->lexer);
    }

    if (pStatement) {
        Script_AddStatement(self->script, pStatement);
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

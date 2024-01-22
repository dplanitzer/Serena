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

static void Parser_Block(ParserRef _Nonnull self, bool isNested, BlockRef _Nullable * _Nonnull pOutBlock);


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
        self->scriptAllocator = NULL;

        free(self);
    }
}

static void Parser_SkipUntilStartOfNextSentence(ParserRef _Nonnull self)
{
    bool done = false;

    while (!done) {
        const Token* t = Lexer_GetToken(&self->lexer);
        
        Lexer_ConsumeToken(&self->lexer);
        switch (t->id) {
            case kToken_Newline:
            case kToken_Semicolon:
            case kToken_Ampersand:
            case kToken_Eof:
                done = true;
                break;

            case kToken_ClosingParenthesis:
                done = true;
                break;

            default:
                break;
        }
    }
}

// nested_block: '(' sentence* ')'
// Expects that the current token on entry is a '(' token.
static void Parser_NestedBlock(ParserRef _Nonnull self, WordRef _Nonnull pWord)
{
    BlockRef pBlock = NULL;
    MorphemeRef pMorpheme = NULL;

    Lexer_ConsumeToken(&self->lexer);

    Parser_Block(self, true, &pBlock);

    const Token* t = Lexer_GetToken(&self->lexer);
    switch (t->id) {
        case kToken_ClosingParenthesis:
            Lexer_ConsumeToken(&self->lexer);
            BlockMorpheme_Create(self->scriptAllocator, pBlock, &pMorpheme);
            Word_AddMorpheme(pWord, pMorpheme);
            break;

        case kToken_Eof:
            printf("Error: unexpected end of script\n");
            break;

        default:
            printf("Error: garbage in nested block\n");
            Parser_SkipUntilStartOfNextSentence(self);
            break;
    }
}

static int GetMorphemeFromToken(TokenId id)
{
    switch (id) {
        case kToken_UnquotedString:     return kMorpheme_UnquotedString;
        case kToken_SingleQuotedString: return kMorpheme_SingleQuotedString;
        case kToken_DoubleQuotedString: return kMorpheme_DoubleQuotedString;
        case kToken_VariableName:       return kMorpheme_VariableReference;
        case kToken_EscapeSequence:     return kMorpheme_EscapeSequence;
        default: return -1;
    }
}

// word: (UNQUOTED_STRING
//          | SINGLE_QUOTED_STRING 
//          | DOUBLE_QUOTED_STRING 
//          | VARIABLE_REFERENCE 
//          | ESCAPE_SEQUENCE 
//          | nested_block
//       )+
// Expects that the current token on entry is a morpheme token.
static bool Parser_Word(ParserRef _Nonnull self, bool isNested, SentenceRef _Nonnull pSentence)
{
    WordRef pWord = NULL;
    bool done = false;
    bool hasError = false;

    Word_Create(self->scriptAllocator, &pWord);

    while (!done) {
        const Token* t = Lexer_GetToken(&self->lexer);

        switch (t->id) {
            case kToken_OpeningParenthesis:
                Parser_NestedBlock(self, pWord);
                break;

            case kToken_Newline:
            case kToken_Semicolon:
            case kToken_Ampersand:
            case kToken_Eof:
                done = true;
                break;

            case kToken_ClosingParenthesis:
                if (isNested) {
                    done = true;
                    break;
                }
                // Fall through

            default: {
                const MorphemeType morphType = GetMorphemeFromToken(t->id);
                if (morphType >= 0) {
                    MorphemeRef pMorpheme = NULL;
                    StringMorpheme_Create(self->scriptAllocator, morphType, t->u.string, &pMorpheme);
                    Word_AddMorpheme(pWord, pMorpheme);

                    if (t->hasTrailingWhitespace) {
                        done = true;
                    }
                    Lexer_ConsumeToken(&self->lexer);
                }
                else {
                    printf("Error: unexpected character '%c'.\n", t->id);
                    hasError = true;
                    done = true;
                }
                break;
            }
        }
    }

    Sentence_AddWord(pSentence, pWord);
    return hasError;
}

// sentenceTerminator: EOF | '\n' | ';' | '&' | ')'
// Also accepts ')' as a sentence terminator if we are currently inside a nested
// sentence
static bool Parser_IsAtSentenceTerminator(ParserRef _Nonnull self, bool isNested)
{
    const Token* t = Lexer_GetToken(&self->lexer);

    switch (t->id) {
        case kToken_Eof:
        case kToken_Newline:
        case kToken_Semicolon:
        case kToken_Ampersand:
            return true;

        case kToken_ClosingParenthesis:
            return isNested;

        default:
            return false;
    }
}

// sentence: word* sentenceTerminator
// Expects that the current token on entry is part of a word.
static void Parser_Sentence(ParserRef _Nonnull self, bool isNested, BlockRef _Nonnull pBlock)
{
    SentenceRef pSentence = NULL;

    Sentence_Create(self->scriptAllocator, &pSentence);

    while (!Parser_IsAtSentenceTerminator(self, isNested)) {
        if(Parser_Word(self, isNested, pSentence)) {
            Parser_SkipUntilStartOfNextSentence(self);
            break;
        }
    }

    // Consume the sentence terminator except if this sentence is nested and it
    // is terminated by a ')' since the closing parenthesis will be consumed by
    // the rule for nested blocks
    const TokenId id = Lexer_GetToken(&self->lexer)->id;
    if (!(isNested && id == kToken_ClosingParenthesis)) {
        Lexer_ConsumeToken(&self->lexer);
    }
    pSentence->terminator = id;

    Block_AddSentence(pBlock, pSentence);
}

// block: sentence*
static void Parser_Block(ParserRef _Nonnull self, bool isNested, BlockRef _Nullable * _Nonnull pOutBlock)
{
    BlockRef pBlock = NULL;

    Block_Create(self->scriptAllocator, &pBlock);

    while (true) {
        const Token* t = Lexer_GetToken(&self->lexer);
        
        if (t->id == kToken_Eof || (isNested && t->id == kToken_ClosingParenthesis)) {
            break;
        }

        Parser_Sentence(self, isNested, pBlock);
    }

    *pOutBlock = pBlock;
}

// Parses the text 'text' and updates the script object 'pScript' to reflect the
// result of parsing 'text'.
void Parser_Parse(ParserRef _Nonnull self, const char* _Nonnull text, ScriptRef _Nonnull pScript)
{
    BlockRef pBlock = NULL;

    Script_Reset(pScript);

    Lexer_SetInput(&self->lexer, text);
    self->scriptAllocator = pScript->allocator;
    Parser_Block(self, false, &pBlock);
    self->scriptAllocator = NULL;
    Script_SetBlock(pScript, pBlock);
    Lexer_SetInput(&self->lexer, NULL);
}

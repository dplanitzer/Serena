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
// MARK: Morpheme
////////////////////////////////////////////////////////////////////////////////

errno_t StringMorpheme_Create(MorphemeType type, const char* _Nonnull pString, MorphemeRef _Nullable * _Nonnull pOutMorpheme)
{
    StringMorpheme* self = (StringMorpheme*)calloc(1, sizeof(StringMorpheme));

    if (self == NULL) {
        *pOutMorpheme = NULL;
        return ENOMEM;
    }

    self->super.type = type;
    self->string = strdup(pString);
    if (self->string == NULL) {
        Morpheme_Destroy((MorphemeRef)self);
        *pOutMorpheme = NULL;
        return ENOMEM;
    }

    *pOutMorpheme = (MorphemeRef)self;
    return 0;
}

errno_t BlockMorpheme_Create(BlockRef _Nonnull pBlock, MorphemeRef _Nullable * _Nonnull pOutMorpheme)
{
    BlockMorpheme* self = (BlockMorpheme*)calloc(1, sizeof(BlockMorpheme));

    if (self == NULL) {
        *pOutMorpheme = NULL;
        return ENOMEM;
    }

    self->super.type = kMorpheme_NestedBlock;
    self->block = pBlock;

    *pOutMorpheme = (MorphemeRef)self;
    return 0;
}

void Morpheme_Destroy(MorphemeRef _Nullable self)
{
    if (self) {
        switch (self->type) {
            case kMorpheme_NestedBlock: {
                BlockMorpheme* mp = (BlockMorpheme*)self;
                Block_Destroy(mp->block);
                mp->block = NULL;
            }
            break;

            default: {
                StringMorpheme* mp = (StringMorpheme*)self;
                free(mp->string);
                mp->string = NULL;
            }
            break;
        }
        free(self);
    }
}

void Morpheme_Print(MorphemeRef _Nonnull self)
{
    printf("{");
    switch (self->type) {
        case kMorpheme_UnquotedString:
            printf("%s", ((StringMorpheme*)self)->string);
            break;

        case kMorpheme_SingleQuotedString:
            printf("'%s'", ((StringMorpheme*)self)->string);
            break;

        case kMorpheme_DoubleQuotedString:
            printf("\"%s\"", ((StringMorpheme*)self)->string);
            break;

        case kMorpheme_EscapeSequence:
            printf("\\%s", ((StringMorpheme*)self)->string);
            break;

        case kMorpheme_VariableReference:
            printf("$%s", ((StringMorpheme*)self)->string);
            break;

        case kMorpheme_NestedBlock:
            printf("(");
            Block_Print(((BlockMorpheme*)self)->block);
            printf(")");
            break;

        default:
            assert(false);
            break;
    }
    printf("}");
}



////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Word
////////////////////////////////////////////////////////////////////////////////

errno_t Word_Create(WordRef _Nullable * _Nonnull pOutWord)
{
    WordRef self = (WordRef)calloc(1, sizeof(Word));

    if (self == NULL) {
        *pOutWord = NULL;
        return ENOMEM;
    }

    *pOutWord = self;
    return 0;
}

void Word_Destroy(WordRef _Nullable self)
{
    if (self) {
        MorphemeRef mp = self->morphemes;

        while(mp) {
            MorphemeRef nx = mp->next;
            Morpheme_Destroy(mp);
            mp = nx;
        }
        self->morphemes = NULL;
        self->lastMorpheme = NULL;
        free(self);
    }
}

void Word_Print(WordRef _Nonnull self)
{
    MorphemeRef mp = self->morphemes;

    while(mp) {
        Morpheme_Print(mp);
        mp = mp->next;
    }
}

void Word_AddMorpheme(WordRef _Nonnull self, MorphemeRef _Nonnull pMorpheme)
{
    if (self->lastMorpheme) {
        (self->lastMorpheme)->next = pMorpheme;
        self->lastMorpheme = pMorpheme;
    }
    else {
        self->morphemes = pMorpheme;
        self->lastMorpheme = pMorpheme;
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Sentence
////////////////////////////////////////////////////////////////////////////////

errno_t Sentence_Create(SentenceRef _Nullable * _Nonnull pOutSentence)
{
    SentenceRef self = (SentenceRef)calloc(1, sizeof(Sentence));

    if (self == NULL) {
        *pOutSentence = NULL;
        return ENOMEM;
    }

    *pOutSentence = self;
    return 0;
}

void Sentence_Destroy(SentenceRef _Nullable self)
{
    if (self) {
        WordRef wd = self->words;

        while(wd) {
            WordRef nx = wd->next;
            Word_Destroy(wd);
            wd = nx;
        }
        self->words = NULL;
        self->lastWord = NULL;
        free(self);
    }
}

void Sentence_Print(SentenceRef _Nonnull self)
{
    WordRef wd = self->words;

    while(wd) {
        Word_Print(wd);
        wd = wd->next;
        if (wd) {
            printf(" ");
        }
    }

    switch (self->terminator) {
        case kToken_Eof:        printf("<EOF>"); break;
        case kToken_Newline:    printf("<NL>"); break;
        case kToken_Semicolon:  printf(";"); break;
        case kToken_Ampersand:  printf("&"); break;
        case kToken_ClosingParenthesis: /* printed by Block */ break;
        default:                printf("<%d>?", self->terminator); break;
    }
}

void Sentence_AddWord(SentenceRef _Nonnull self, WordRef _Nonnull pWord)
{
    if (self->lastWord) {
        (self->lastWord)->next = pWord;
        self->lastWord = pWord;
    }
    else {
        self->words = pWord;
        self->lastWord = pWord;
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Block
////////////////////////////////////////////////////////////////////////////////

errno_t Block_Create(BlockRef _Nullable * _Nonnull pOutBlock)
{
    BlockRef self = (BlockRef)calloc(1, sizeof(Block));

    if (self == NULL) {
        *pOutBlock = NULL;
        return ENOMEM;
    }

    *pOutBlock = self;
    return 0;
}

void Block_Destroy(BlockRef _Nullable self)
{
    if (self) {
        SentenceRef st = self->sentences;

        while(st) {
            SentenceRef nx = st->next;
            Sentence_Destroy(st);
            st = nx;
        }
        self->sentences = NULL;
        self->lastSentence = NULL;
        free(self);
    }
}

void Block_Print(BlockRef _Nonnull self)
{
    SentenceRef st = self->sentences;

    while(st) {
        Sentence_Print(st);
        st = st->next;
        if (st) {
            printf("\n");
        }
    }
}

void Block_AddSentence(BlockRef _Nonnull self, SentenceRef _Nonnull pSentence)
{
    if (self->lastSentence) {
        (self->lastSentence)->next = pSentence;
        self->lastSentence = pSentence;
    }
    else {
        self->sentences = pSentence;
        self->lastSentence = pSentence;
    }
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
    Block_Destroy(self->block);
    self->block = NULL;
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
    Block_Print(self->block);
    printf("\n");
}

void Script_SetBlock(ScriptRef _Nonnull self, BlockRef _Nonnull pBlock)
{
    if (self->block != pBlock) {
        Block_Destroy(self->block);
        self->block = pBlock;
    }
}

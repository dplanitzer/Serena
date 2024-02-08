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

errno_t StringMorpheme_Create(StackAllocatorRef _Nonnull pAllocator, MorphemeType type, const char* _Nonnull pString, MorphemeRef _Nullable * _Nonnull pOutMorpheme)
{
    size_t len = strlen(pString);
    StringMorpheme* self = (StringMorpheme*)StackAllocator_ClearAlloc(pAllocator, sizeof(StringMorpheme) + len);

    if (self == NULL) {
        *pOutMorpheme = NULL;
        return ENOMEM;
    }

    self->super.type = type;
    strcpy(self->string, pString);

    *pOutMorpheme = (MorphemeRef)self;
    return 0;
}

errno_t BlockMorpheme_Create(StackAllocatorRef _Nonnull pAllocator, BlockRef _Nonnull pBlock, MorphemeRef _Nullable * _Nonnull pOutMorpheme)
{
    BlockMorpheme* self = (BlockMorpheme*)StackAllocator_ClearAlloc(pAllocator, sizeof(BlockMorpheme));

    if (self == NULL) {
        *pOutMorpheme = NULL;
        return ENOMEM;
    }

    self->super.type = kMorpheme_NestedBlock;
    self->block = pBlock;

    *pOutMorpheme = (MorphemeRef)self;
    return 0;
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

errno_t Word_Create(StackAllocatorRef _Nonnull pAllocator, WordRef _Nullable * _Nonnull pOutWord)
{
    WordRef self = (WordRef)StackAllocator_ClearAlloc(pAllocator, sizeof(Word));

    *pOutWord = self;
    return (self) ? 0 : ENOMEM;
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

errno_t Sentence_Create(StackAllocatorRef _Nonnull pAllocator, SentenceRef _Nullable * _Nonnull pOutSentence)
{
    SentenceRef self = (SentenceRef)StackAllocator_ClearAlloc(pAllocator, sizeof(Sentence));

    *pOutSentence = self;
    return (self) ? 0 : ENOMEM;
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

int Sentence_GetWordCount(SentenceRef _Nonnull self)
{
    WordRef curWord = self->words;
    int count = 0;

    while (curWord) {
        curWord = curWord->next;
        count++;
    }
    return count;
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

errno_t Block_Create(StackAllocatorRef _Nonnull pAllocator, BlockRef _Nullable * _Nonnull pOutBlock)
{
    BlockRef self = (BlockRef)StackAllocator_ClearAlloc(pAllocator, sizeof(Block));

    *pOutBlock = self;
    return (self) ? 0 : ENOMEM;
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
    if (StackAllocator_Create(512, 4096, &self->allocator) != 0) {
        Script_Destroy(self);
        *pOutScript = NULL;
        return ENOMEM;
    }

    *pOutScript = self;
    return 0;
}

void Script_Reset(ScriptRef _Nonnull self)
{
    StackAllocator_DeallocAll(self->allocator);
    self->block = NULL;
}

void Script_Destroy(ScriptRef _Nullable self)
{
    if (self) {
        StackAllocator_Destroy(self->allocator);
        self->allocator = NULL;
        self->block = NULL;
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
    self->block = pBlock;
}

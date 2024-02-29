//
//  Script.h
//  sh
//
//  Created by Dietmar Planitzer on 1/2/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Script_h
#define Script_h

#include <System/System.h>
#include "Lexer.h"
#include "StackAllocator.h"

struct Morpheme;
typedef struct Morpheme* MorphemeRef;

struct Word;
typedef struct Word* WordRef;

struct Sentence;
typedef struct Sentence* SentenceRef;

struct Block;
typedef struct Block* BlockRef;

struct Script;
typedef struct Script* ScriptRef;



typedef enum MorphemeType {
    kMorpheme_UnquotedString,           // StringMorpheme
    kMorpheme_SingleQuotedString,       // StringMorpheme
    kMorpheme_DoubleQuotedString,       // StringMorpheme
    kMorpheme_QuotedCharacter,          // StringMorpheme
    kMorpheme_VariableReference,        // StringMorpheme
    kMorpheme_NestedBlock,              // BlockMorpheme
} MorphemeType;

typedef struct Morpheme {
    MorphemeRef _Nullable   next;
    MorphemeType            type;
} Morpheme;

typedef struct StringMorpheme {
    Morpheme        super;
    char            string[1];
} StringMorpheme;

typedef struct BlockMorpheme {
    Morpheme            super;
    BlockRef _Nonnull   block;
} BlockMorpheme;

extern errno_t StringMorpheme_Create(StackAllocatorRef _Nonnull pAllocator, MorphemeType type, const char* _Nonnull pString, MorphemeRef _Nullable * _Nonnull pOutMorpheme);
extern errno_t BlockMorpheme_Create(StackAllocatorRef _Nonnull pAllocator, BlockRef _Nonnull pBlock, MorphemeRef _Nullable * _Nonnull pOutMorpheme);  // Takes ownership of the block
extern void Morpheme_Print(MorphemeRef _Nonnull self);



typedef struct Word {
    WordRef _Nullable       next;
    MorphemeRef _Nonnull    morphemes;
    MorphemeRef _Nonnull    lastMorpheme;
} Word;

extern errno_t Word_Create(StackAllocatorRef _Nonnull pAllocator, WordRef _Nullable * _Nonnull pOutWord);
extern void Word_Print(WordRef _Nonnull self);
extern void Word_AddMorpheme(WordRef _Nonnull self, MorphemeRef _Nonnull pMorpheme);



typedef struct Sentence {
    SentenceRef _Nullable   next;
    WordRef _Nonnull        words;
    WordRef _Nonnull        lastWord;
    TokenId                 terminator;
} Sentence;

extern errno_t Sentence_Create(StackAllocatorRef _Nonnull pAllocator, SentenceRef _Nullable * _Nonnull pOutSentence);
extern void Sentence_Print(SentenceRef _Nonnull self);
extern int Sentence_GetWordCount(SentenceRef _Nonnull self);
extern void Sentence_AddWord(SentenceRef _Nonnull self, WordRef _Nonnull pWord);



typedef struct Block {
    SentenceRef _Nullable   sentences;
    SentenceRef _Nullable   lastSentence;
} Block;

extern errno_t Block_Create(StackAllocatorRef _Nonnull pAllocator, BlockRef _Nullable * _Nonnull pOutBlock);
extern void Block_Print(BlockRef _Nonnull self);
extern void Block_AddSentence(BlockRef _Nonnull self, SentenceRef _Nonnull pSentence);



typedef struct Script {
    BlockRef _Nullable          block;
    StackAllocatorRef _Nonnull  allocator;
} Script;

// The script manages a stack allocator which is used to store all nodes in the
// AST that the script manages.
extern errno_t Script_Create(ScriptRef _Nullable * _Nonnull pOutScript);
extern void Script_Destroy(ScriptRef _Nullable self);
extern void Script_Reset(ScriptRef _Nonnull self);
extern void Script_Print(ScriptRef _Nonnull self);
extern void Script_SetBlock(ScriptRef _Nonnull self, BlockRef _Nonnull pBlock);

#endif  /* Script_h */

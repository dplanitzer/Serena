//
//  Lexer.h
//  sh
//
//  Created by Dietmar Planitzer on 1/2/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <apollo/apollo.h>


typedef enum _TokenId {
    kToken_Eof = 0,         // End of file
    kToken_Eos,             // End of statement
    kToken_Character,       // Some character that doesn't start any of the other tokens (note that this includes things like ASCII control codes)
    kToken_Word,
} TokenId;

typedef struct _Token {
    TokenId         id;
    union {
        struct Word {
            const char* text;
            int         length;
        }       word;
        char    character;
    }               u;
} Token;


typedef struct _Lexer {
    const char* _Nonnull    text;
    int                     textIndex;

    char* _Nonnull          wordBuffer;
    int                     wordBufferCapacity;     // max number of characters we can store excluding the trailing NUL
    int                     wordBufferCount;

    Token                   t;
} Lexer;
typedef Lexer* LexerRef;


extern errno_t Lexer_Init(LexerRef _Nonnull self);
extern void Lexer_Deinit(LexerRef _Nonnull self);

// Sets the lexer input. Note that the lexer maintains a reference to the input
// text. It does not copy it.
extern void Lexer_SetInput(LexerRef _Nonnull self, const char* _Nullable text);

// Returns the token at the current lexer position. This function does not
// consume the token. The caller must copy whatever data it wants to retain.
#define Lexer_GetToken(__self) \
    ((const Token*)&(__self)->t)

// Consumes the current token and advances the current lexer position.
extern void Lexer_ConsumeToken(LexerRef _Nonnull self);

//
//  Lexer.h
//  sh
//
//  Created by Dietmar Planitzer on 1/2/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Lexer_h
#define Lexer_h

#include <System/System.h>
#include <stdbool.h>


typedef enum TokenId {
    kToken_Eof = 0,                     // -            End of file
    kToken_Character,                   // character    Some character that doesn't start any of the other tokens (note that this includes things like ASCII control codes)
    kToken_UnquotedString,              // string
    kToken_SingleQuotedString,          // string
    kToken_DoubleQuotedString,          // string
    kToken_VariableName,                // string
    kToken_EscapeSequence,              // string
    kToken_Semicolon = ';',             // -
    kToken_Newline = '\n',              // -
    kToken_OpeningParenthesis = '(',    // -
    kToken_ClosingParenthesis = ')',    // -
    kToken_Less = '<',                  // -
    kToken_Greater = '>',               // -
    kToken_Bar = '|',                   // -
    kToken_Ampersand = '&'              // -
} TokenId;

typedef struct _Token {
    TokenId         id;
    union {
        const char* string;
        char        character;
    }               u;
    int             column;     // column & line at the start of the token
    int             line;
    int             length;     // token length in terms of characters
    bool            hasTrailingWhitespace;
} Token;


typedef struct _Lexer {
    const char* _Nonnull    source;
    int                     sourceIndex;

    char* _Nonnull          textBuffer;
    int                     textBufferCapacity;     // max number of characters we can store excluding the trailing NUL
    int                     textBufferCount;

    int                     column;                 // current column & line
    int                     line;
    
    Token                   t;
} Lexer;
typedef Lexer* LexerRef;


extern errno_t Lexer_Init(LexerRef _Nonnull self);
extern void Lexer_Deinit(LexerRef _Nonnull self);

// Sets the lexer input. Note that the lexer maintains a reference to the input
// text. It does not copy it.
extern void Lexer_SetInput(LexerRef _Nonnull self, const char* _Nullable source);

// Returns the token at the current lexer position. This function does not
// consume the token. The caller must copy whatever data it wants to retain.
#define Lexer_GetToken(__self) \
    ((const Token*)&(__self)->t)

// Consumes the current token and advances the current lexer position.
extern void Lexer_ConsumeToken(LexerRef _Nonnull self);

#endif  /* Lexer_h */

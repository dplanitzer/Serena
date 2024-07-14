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
#include <stdio.h>


typedef enum TokenId {
    // Default Mode
    kToken_Eof = EOF,                   // -            End of file
    kToken_Ampersand = '&',             // -
    kToken_Assignment = '=',            // -
    kToken_Asterisk = '*',              // -
    kToken_Bang = '!',                  // -
    kToken_Bar = '|',                   // -
    kToken_ClosingBrace = '}',          // -
    kToken_ClosingParenthesis = ')',    // -
    kToken_Conjunction = 256,           // -
    kToken_Disjunction = 257,           // - 
    kToken_DoubleBacktick = 258,        // -
    kToken_DoubleQuote = '"',           // - 
    kToken_EqualEqual = 259,            // -
    kToken_Greater = '>',               // -
    kToken_GreaterEqual = 260,          // -
    kToken_Less = '<',                  // -
    kToken_LessEqual = 261,             // -
    kToken_Minus = '-',                 // -
    kToken_Newline = '\n',              // -
    kToken_NotEqual = 262,              // -
    kToken_OpeningBrace = '{',          // -
    kToken_OpeningParenthesis = '(',    // -
    kToken_Plus = '+',                  // -
    kToken_Semicolon = ';',             // -
    kToken_Slash = '/',                 // -

    kToken_BacktickString = 263,        // u.string
    kToken_EscapeSequence = 264,        // u.string
    kToken_Identifier = 265,            // u.string
    kToken_SingleQuoteString = 266,     // u.string
    kToken_VariableName = 267,          // u.string 'foo:bar'

    // DQ, DBT Mode
    // kToken_Eof
    // kToken_ClosingParenthesis
    // kToken_DoubleBacktick
    // kToken_DoubleQuote
    // kToken_EscapeSequence
    // kToken_VariableName
    kToken_EscapedExpression = 268,     // -
    kToken_StringSegment = 269,         // u.string
} TokenId;


typedef enum LexerMode {
    kLexerMode_Default,
    kLexerMode_DoubleQuote,
    kLexerMode_DoubleBacktick
} LexerMode;


typedef struct Token {
    TokenId     id;
    union {
        const char* string;
    }           u;
    int         column;                 // column & line at the start of the token
    int         line;
    int         length;                 // token length in terms of characters
    bool        hasLeadingWhitespace;
    bool        isIncomplete;           // escape sequence, backtick string, single quote string: true if token is incomplete, i.e. because of hitting EOF before the end of the token; false otherwise
} Token;


typedef struct Lexer {
    const char* _Nonnull    source;
    int                     sourceIndex;

    char* _Nonnull          textBuffer;
    int                     textBufferCapacity;     // max number of characters we can store excluding the trailing NUL
    int                     textBufferCount;

    int                     column;                 // current column & line
    int                     line;
    
    Token                   t;

    LexerMode               mode;
} Lexer;


extern void Lexer_Init(Lexer* _Nonnull self);
extern void Lexer_Deinit(Lexer* _Nonnull self);

// Sets the lexer input. Note that the lexer maintains a reference to the input
// text. It does not copy it.
extern void Lexer_SetInput(Lexer* _Nonnull self, const char* _Nullable source);

#define Lexer_GetMode(__self) \
    (__self)->mode

// Changes the lexer mode to '__mode'. Note that you must switch mode before
// consuming the current token if the next token should be lexed based on the
// new mode.
#define Lexer_SetMode(__self, __mode) \
    (__self)->mode = __mode

// Returns the token at the current lexer position. This function does not
// consume the token. The caller must copy whatever data it wants to retain.
#define Lexer_GetToken(__self) \
    ((const Token*)&(__self)->t)

// Consumes the current token and advances the current lexer position.
extern void Lexer_ConsumeToken(Lexer* _Nonnull self);

#endif  /* Lexer_h */

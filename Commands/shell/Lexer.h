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
#include <stdint.h>
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

    kToken_Integer = 263,               // u.i32
    kToken_BacktickString = 264,        // u.string
    kToken_Identifier = 265,            // u.string
    kToken_SingleQuoteString = 266,     // u.string
    kToken_VariableName = 267,          // u.string 'foo:bar'

    kToken_Else = 268,                  // u.string
    kToken_If = 269,                    // u.string
    kToken_Internal = 270,              // u.string
    kToken_Let = 271,                   // u.string
    kToken_Public = 272,                // u.string
    kToken_Var = 273,                   // u.string
    kToken_While = 274,                 // u.string
    kToken_False = 275,                 // u.string
    kToken_True = 276,                  // u.string
    kToken_Break = 277,                 // u.string
    kToken_Continue = 278,              // u.string
    
    
    // DQ, DBT Mode
    // kToken_Eof
    // kToken_ClosingParenthesis
    // kToken_DoubleBacktick
    // kToken_DoubleQuote
    // kToken_VariableName
    kToken_EscapeSequence = 512,        // u.string
    kToken_EscapedExpression = 513,     // -
    kToken_StringSegment = 514,         // u.string
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
        int32_t     i32;
    }           u;
    int         column;                 // column & line at the start of the token
    int         line;
    int         length;                 // token length in terms of characters
    bool        hasLeadingWhitespace;
    bool        isIncomplete;           // identifier, escape sequence, backtick string, single quote string: true if token is incomplete; false otherwise
} Token;

// Incomplete identifier:
// Is an identifier where the last character is the '\' of a escaped character
// and the actual character to escape is missing because the lexer encountered
// EOF before that character.
//
// Incomplete ' and ` string:
// Is a string where the closing ' or ` is missing because the lexer encountered
// EOF before the closing quote.
//
// Incomplete escape sequence:
// Is an escape sequence where the actual character to escape is missing because
// the lexer encountered EOF before that character.


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

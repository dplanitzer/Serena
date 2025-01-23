//
//  Lexer.c
//  sh
//
//  Created by Dietmar Planitzer on 1/2/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Lexer.h"
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_TEXT_BUFFER_CAPACITY    128


typedef struct Keyword {
    const char* _Nonnull    kw;
    TokenId                 id;
} Keyword;

static const Keyword gKeywords[] = {
    { "break",      kToken_Break },
    { "continue",   kToken_Continue },
    { "else",       kToken_Else },
    { "false",      kToken_False },
    { "if",         kToken_If },
    { "internal",   kToken_Internal },
    { "let",        kToken_Let },
    { "public",     kToken_Public },
    { "true",       kToken_True },
    { "var",        kToken_Var },
    { "while",      kToken_While },
};


void Lexer_Init(Lexer* _Nonnull self)
{
    self->source = "";
    self->sourceIndex = 0;
    self->textBuffer = NULL;
    self->textBufferCapacity = 0;
    self->textBufferCount = 0;
    self->column = 1;
    self->line = 1;
    self->t.id = kToken_Eof;
    self->mode = kLexerMode_Default;
}

void Lexer_Deinit(Lexer* _Nonnull self)
{
    self->source = NULL;
    free(self->textBuffer);
    self->textBuffer = NULL;
}

// Sets the lexer input. Note that the lexer maintains a reference to the input
// text. It does not copy it.
void Lexer_SetInput(Lexer* _Nonnull self, const char* _Nullable source)
{
    self->source = (source) ? source : "";
    self->sourceIndex = 0;

    // Get the first token
    Lexer_ConsumeToken(self);
}

static void Lexer_AddCharToTextBuffer(Lexer* _Nonnull self, char ch)
{
    if (self->textBufferCount >= self->textBufferCapacity) {
        const size_t newCapacity = (self->textBufferCapacity > 0) ? self->textBufferCapacity * 2 : INITIAL_TEXT_BUFFER_CAPACITY;
        char* pNewTextBuffer = realloc(self->textBuffer, newCapacity);

        assert(pNewTextBuffer != NULL);
        self->textBuffer = pNewTextBuffer;
        self->textBufferCapacity = newCapacity;
    }

    self->textBuffer[self->textBufferCount++] = ch;
}

// Scans a variable name. Expects that the current input position is at the first
// character of the variable name.
static void Lexer_ScanVariableName(Lexer* _Nonnull self)
{
    self->textBufferCount = 0;

    while (true) {
        const char ch = self->source[self->sourceIndex];

        if (ch == '\0' || (!isalnum(ch) && ch != '_')) {
            break;
        }

        self->sourceIndex++;
        self->column++;
        Lexer_AddCharToTextBuffer(self, ch);
    }

    Lexer_AddCharToTextBuffer(self, '\0');
}

// Scans a single quoted/backticked string. Expects that the current input
// position is at the first character of the string contents.
static bool Lexer_ScanString(Lexer* _Nonnull self, char closingMark)
{
    bool isIncomplete = false;

    self->textBufferCount = 0;

    while (true) {
        const char ch = self->source[self->sourceIndex];

        if (ch == '\0') {
            isIncomplete = true;
            break;
        }

        self->sourceIndex++;
        self->column++;

        if (ch == closingMark) {
            break;
        }
        Lexer_AddCharToTextBuffer(self, ch);
    }

    Lexer_AddCharToTextBuffer(self, '\0');
    return isIncomplete;
}

// Scans an octal code escape sequence of one, two or three digits into the text
// buffer at its current position. Expects that the current input position is at
// the first (valid) digit
static void Lexer_ScanOctalEscapeSequence(Lexer* _Nonnull self)
{
    int val = 0;

    for (int i = 0; i < 3; i++) {
        char ch = self->source[self->sourceIndex];

        if (ch < '0' || ch > '7') {
            break;
        }

        self->sourceIndex++;
        self->column++;
        val = (val << 3) + (ch - '0');
    }

    Lexer_AddCharToTextBuffer(self, val & 0xff);
}

// Scans a single byte escape code in the form of a hexadecimal number. Expects
// that the current input position is at the first (valid) digit.
static void Lexer_ScanHexByteEscapeSequence(Lexer* _Nonnull self)
{
    int val = 0;

    for (int i = 0; i < 2; i++) {
        char ch = self->source[self->sourceIndex];

        if (!isxdigit(ch)) {
            break;
        }

        self->sourceIndex++;
        self->column++;
        const int d = (ch >= 'a') ? ch - 'a' : ((ch >= 'A') ? ch - 'A' : ch - '0');
        val = (val << 4) + d;
    }

    Lexer_AddCharToTextBuffer(self, val & 0xff);
}

// Scans an escape sequence that appears inside of a " or `` string. Expects
// that the current input position is at the first character following the
// initial '\' character.
static bool Lexer_ScanStringEscapeSequence(Lexer* _Nonnull self)
{
    char ch = self->source[self->sourceIndex];

    switch (ch) {
        case 'a':   ch = 0x07;  break;
        case 'b':   ch = 0x08;  break;
        case 'e':   ch = 0x1b;  break;
        case 'f':   ch = 0x0c;  break;
        case 'r':   ch = 0x0d;  break;
        case 'n':   ch = 0x0a;  break;
        case 'v':   ch = 0x0b;  break;

        case '$':   break;
        case '"':   break;
        case '\'':  break;
        case '\\':  break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
            Lexer_ScanOctalEscapeSequence(self);
            return false;

        case 'x':
        case 'X':
            self->sourceIndex++;
            self->column++;
            Lexer_ScanHexByteEscapeSequence(self);
            return false;
            
        // XXX add \uxxxx and \Uxxxxyyyy (Unicode) support

        case '\0':
            return true;

        case '\r':
            if (self->source[self->sourceIndex + 1] != '\n') {
                // not CRLF
                break;
            }
            // CRLF
            self->sourceIndex++;
            // Fall through
            
        case '\n':
            self->column = 1;
            self->line++;
            ch = '\n';
            break;

        default:
            self->sourceIndex++;
            self->column++;
            return false;
    }

    self->sourceIndex++;
    Lexer_AddCharToTextBuffer(self, ch);
    return false;
}

// Scans a string segment inside a " or `` string. Expects that the current
// input position is at the first character of the string contents.
static void Lexer_ScanStringSegment(Lexer* _Nonnull self)
{
    LexerMode mode = self->mode;

    self->textBufferCount = 0;

    for (;;) {
        const char ch = self->source[self->sourceIndex];

        if (ch == '\0' || ch == '$' || ch == '\\') {
            break;
        }
        if (ch == '"' && mode == kLexerMode_DoubleQuote) {
            break;
        }
        if (ch == '`' && self->source[self->sourceIndex + 1] == '`' && mode == kLexerMode_DoubleBacktick) {
            break;
        }

        self->sourceIndex++;
        self->column++;
        Lexer_AddCharToTextBuffer(self, ch);
    }

    Lexer_AddCharToTextBuffer(self, '\0');
}

// Tries scanning a variable name of the form:
// '$' (('_' | [a-z] | [A-Z] | [0-9])* ':')? ('_' | [a-z] | [A-Z] | [0-9])+
// 
// Expects that the current input position is at the '$' character.
static bool Lexer_TryScanVariableName(Lexer* _Nonnull self)
{
    size_t nameLen = 0;
    int savedSourceIndex = self->sourceIndex;
    int savedColumn = self->column;

    self->textBufferCount = 0;

    // Consume '$'
    self->sourceIndex++;
    self->column++;

    while (true) {
        const char ch = self->source[self->sourceIndex];

        if (!isalnum(ch) && ch != '_') {
            break;
        }

        self->sourceIndex++;
        self->column++;
        nameLen++;
        Lexer_AddCharToTextBuffer(self, ch);
    }

    if (self->source[self->sourceIndex] == ':') {
        self->sourceIndex++;
        self->column++;
        Lexer_AddCharToTextBuffer(self, ':');
        nameLen = 0;

        while (true) {
            const char ch = self->source[self->sourceIndex];

            if (!isalnum(ch) && ch != '_') {
                break;
            }

            self->sourceIndex++;
            self->column++;
            nameLen++;
            Lexer_AddCharToTextBuffer(self, ch);
        }
    }

    Lexer_AddCharToTextBuffer(self, '\0');

    if (nameLen > 0) {
        return true;
    }
    else {
        self->sourceIndex = savedSourceIndex;
        self->column = savedColumn;
        return false;
    }
}

// Scans an escaped character. Expects that the current input position is at the
// first character following the initial '\' character.
static bool Lexer_ScanEscapedCharacter(Lexer* _Nonnull self)
{
    const char ch = self->source[self->sourceIndex];

    switch (ch) {
        case '\0':
            return true;

        case '\r':
            if (self->source[self->sourceIndex + 1] != '\n') {
                // not CRLF
                break;
            }
            // CRLF
            self->sourceIndex++;
            // Fall through
            
        case '\n':
            self->column = 1;
            self->line++;
            self->sourceIndex++;
            // Drop the escaped newline
            return false;

        default:
            break;
    }

    self->column++;
    self->sourceIndex++;
    Lexer_AddCharToTextBuffer(self, ch);
    return false;
}

// Returns true if the given character should terminate the construction of an
// identifier character; false otherwise.
static bool isIdentifierTerminator(char ch)
{
    switch (ch) {
        case '\0':
        case '|':
        case '&':
        case '+':
        case '-':
        case '*':
        case '/':
        case '%':
        case ';':
        case '$':
        case '"':
        case '`':
        case '\'':
        case '(':
        case ')':
        case '{':
        case '}':
        case '<':
        case '>':
        case '=':
        case '!':
            return true;

        default:
            return (isspace(ch)) ? true : false;
    }
}

// Scans an identifier of the form:
// [^\0x20\0x09|&+-*/%\;$"`'(){}<>=!_a-zA-Z0-9]+
// 
// Expects that the current input position is at the first character of the
// identifier. This first character is accepted no matter what. Even if it would
// be treated as a terminating character otherwise.
static bool Lexer_ScanIdentifier(Lexer* _Nonnull self)
{
    bool isIncomplete = false;

    self->textBufferCount = 0;

    if (self->source[self->sourceIndex] != '\\') {
        Lexer_AddCharToTextBuffer(self, self->source[self->sourceIndex]);
        self->column++;
        self->sourceIndex++;
    }

    while (true) {
        const char ch = self->source[self->sourceIndex];

        if (isIdentifierTerminator(ch)) {
            break;
        }

        self->sourceIndex++;
        self->column++;

        if (ch != '\\') {
            Lexer_AddCharToTextBuffer(self, ch);
        }
        else if (Lexer_ScanEscapedCharacter(self)) {
            isIncomplete = true;
            break;
        }
    }

    Lexer_AddCharToTextBuffer(self, '\0');
    return isIncomplete;
}

static int kw_cmp(const char* _Nonnull lhs, const Keyword* _Nonnull rhs)
{
    return strcmp(lhs, rhs->kw);
}

static TokenId Lexer_GetIdentifierTokenId(Lexer* _Nonnull self)
{
    const Keyword* kw = NULL;

    if (self->textBufferCount > 2 && isalpha(self->textBuffer[0])) {
        kw = bsearch(self->textBuffer, gKeywords, sizeof(gKeywords) / sizeof(Keyword), sizeof(Keyword), (int (*)(const void*, const void*))kw_cmp);
    }

    return (kw) ? kw->id : kToken_Identifier;
}

// Scans a positive integer literal. Expects that the current input position is
// at the first digit.
// XXX do the parsing ourselves so that we can handle escaped digits correctly
static int32_t Lexer_ScanInteger(Lexer* _Nonnull self)
{
    const char* sp = &self->source[self->sourceIndex];
    char* ep;
    unsigned long long val = strtoull(sp, &ep, 10);
    const size_t ndigits = ep - sp;

    self->sourceIndex += ndigits;
    self->column += ndigits;
    while(isdigit(self->source[self->sourceIndex])) {
        self->sourceIndex++;
        self->column++;
    }

    if (val > INT32_MAX) {
        val = INT32_MAX;
    }
    return (int32_t)val;
}

static void Lexer_SkipWhitespace(Lexer* _Nonnull self)
{
    while (true) {
        const char ch = self->source[self->sourceIndex];

        if (ch == '\0' || !isspace(ch)) {
            break;
        }

        self->sourceIndex++;
        self->column++;
    }
}

static void Lexer_SkipLineComment(Lexer* _Nonnull self)
{
    while (true) {
        const char ch = self->source[self->sourceIndex];

        if (ch == '\0' || ch == '\n' || (ch == '\r' && self->source[self->sourceIndex + 1] == '\n')) {
            break;
        }

        self->sourceIndex++;
        self->column++;
    }
}

static void Lexer_ConsumeToken_DefaultMode(Lexer* _Nonnull self)
{
    const char c = self->source[self->sourceIndex];

    self->t.column = self->column;
    self->t.line = self->line;
    self->t.length = 0;
    self->t.u.string = NULL;
    self->t.hasLeadingWhitespace = (c == '#' || isspace(c) || self->sourceIndex == 0);
    self->t.isIncomplete = false;
    
    while (true) {
        const char ch = self->source[self->sourceIndex];

        switch (ch) {
            case '\0':
                self->t.id = kToken_Eof;
                return;

            case ' ':
            case '\t':
            case '\v':
            case '\f':
                Lexer_SkipWhitespace(self);
                break;

            case '#':
                Lexer_SkipLineComment(self);
                break;

            case '\r':
                self->sourceIndex++;
                self->column = 1;
                if (self->source[self->sourceIndex + 1] != '\n') {
                    break;
                }
                // Fall through

            case '\n':
                self->sourceIndex++;
                self->column = 1;
                self->line++;

                self->t.id = kToken_Newline;
                self->t.length = (ch == '\n') ? 1 : 2;
                return;

            case '(':
            case ')':
            case '{':
            case '}':
            case '+':
            case '-':
            case '*':
            case '/':
            case '%':
            case '"':
            case ';':
                self->sourceIndex++;
                self->column++;

                self->t.id = ch;
                return;

            case '&':
                self->sourceIndex++;
                self->column++;

                if (self->source[self->sourceIndex] == '&') {
                    self->sourceIndex++;
                    self->column++;
                    self->t.id = kToken_Conjunction;
                } else {
                    self->t.id = ch;        // &
                }
                return;

            case '|':
                self->sourceIndex++;
                self->column++;

                if (self->source[self->sourceIndex] == '|') {
                    self->sourceIndex++;
                    self->column++;
                    self->t.id = kToken_Disjunction;
                } else {
                    self->t.id = ch;        // |
                }
                return;

            case '<':
            case '>':
                self->sourceIndex++;
                self->column++;

                if (self->source[self->sourceIndex] == '=') {
                    self->sourceIndex++;
                    self->column++;
                    self->t.id = (ch == '<') ? kToken_LessEqual : kToken_GreaterEqual;
                } else {
                    self->t.id = ch;        // < or >
                }
                return;

            case '!':
                self->sourceIndex++;
                self->column++;

                if (self->source[self->sourceIndex] == '=') {
                    self->sourceIndex++;
                    self->column++;
                    self->t.id = kToken_NotEqual;
                } else {
                    self->t.id = ch;        // !
                }
                return;

            case '=':
                self->sourceIndex++;
                self->column++;

                if (self->source[self->sourceIndex] == '=') {
                    self->sourceIndex++;
                    self->column++;
                    self->t.id = kToken_EqualEqual;
                } else {
                    self->t.id = ch;        // =
                }
                return;

            case '`':
                self->sourceIndex++;
                self->column++;

                if (self->source[self->sourceIndex] == '`') {
                    self->sourceIndex++;
                    self->column++;
                    self->t.id = kToken_DoubleBacktick;
                } else {
                    self->t.id = kToken_BacktickString;
                    self->t.isIncomplete = Lexer_ScanString(self, '`');
                    self->t.u.string = self->textBuffer;
                    self->t.length = self->textBufferCount - 1;
                }
                return;

            case '\'':
                self->sourceIndex++;
                self->column++;

                self->t.id = kToken_SingleQuoteString;
                self->t.isIncomplete = Lexer_ScanString(self, '\'');
                self->t.u.string = self->textBuffer;
                self->t.length = self->textBufferCount - 1;
                return;

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                self->t.id = kToken_Integer;
                self->t.u.i32 = Lexer_ScanInteger(self);
                return;

            default:
                if (ch == '$') {
                    if (Lexer_TryScanVariableName(self)) {
                        self->t.id = kToken_VariableName;
                        self->t.u.string = self->textBuffer;
                        self->t.length = self->textBufferCount - 1;
                        return;
                    }
                }

                self->t.isIncomplete = Lexer_ScanIdentifier(self);
                self->t.id = Lexer_GetIdentifierTokenId(self);
                self->t.u.string = self->textBuffer;
                self->t.length = self->textBufferCount - 1;
                return;
        }
    }
}

static void Lexer_ConsumeToken_StringMode(Lexer* _Nonnull self)
{
    const char ch = self->source[self->sourceIndex];

    self->t.column = self->column;
    self->t.line = self->line;
    self->t.length = 0;
    self->t.u.string = NULL;
    self->t.hasLeadingWhitespace = false;
    self->t.isIncomplete = false;

    if (ch == '"' && self->mode == kLexerMode_DoubleQuote) {
        self->sourceIndex++;
        self->column++;

        self->t.id = kToken_DoubleQuote;
        return;
    }
    else if (ch == '`' && self->source[self->sourceIndex + 1] == '`' && self->mode == kLexerMode_DoubleBacktick) {
        self->sourceIndex += 2;
        self->column += 2;

        self->t.id = kToken_DoubleBacktick;
        return;
    }

    switch (ch) {
        case '\0':
            self->t.id = kToken_Eof;
            break;

        case '\\':
            self->sourceIndex++;
            self->column++;

            if (self->source[self->sourceIndex] == '(') {
                self->sourceIndex++;
                self->column++;
        
                self->t.id = kToken_EscapedExpression;
            }
            else {
                self->textBufferCount = 0;
                self->t.isIncomplete = Lexer_ScanStringEscapeSequence(self);
                Lexer_AddCharToTextBuffer(self, '\0');

                self->t.id = kToken_EscapeSequence;
                self->t.u.string = self->textBuffer;
                self->t.length = self->textBufferCount - 1;
            }
            break;

        case '$':
            if (Lexer_TryScanVariableName(self)) {
                self->t.id = kToken_VariableName;
                self->t.u.string = self->textBuffer;
                self->t.length = self->textBufferCount - 1;
                return;
            }
            // fall through

        default:
            Lexer_ScanStringSegment(self);
            self->t.id = kToken_StringSegment;
            self->t.u.string = self->textBuffer;
            self->t.length = self->textBufferCount - 1;
            break;
    }
}

void Lexer_ConsumeToken(Lexer* _Nonnull self)
{
    switch (self->mode) {
        case kLexerMode_Default:
            Lexer_ConsumeToken_DefaultMode(self);
            break;

        case kLexerMode_DoubleBacktick:
        case kLexerMode_DoubleQuote:
            Lexer_ConsumeToken_StringMode(self);
            break;

        default:
            abort();
            break;
    }
}

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
#include <stdio.h>
#include <string.h>

#define INITIAL_TEXT_BUFFER_CAPACITY    16


errno_t Lexer_Init(LexerRef _Nonnull self)
{
    memset(self, 0, sizeof(Lexer));

    self->source = "";
    self->sourceIndex = 0;
    self->textBufferCapacity = 0;
    self->textBufferCount = 0;
    self->column = 1;
    self->line = 1;
    self->t.id = kToken_Eof;
    
    return 0;
}

void Lexer_Deinit(LexerRef _Nonnull self)
{
    self->source = NULL;
    free(self->textBuffer);
    self->textBuffer = NULL;
}

// Sets the lexer input. Note that the lexer maintains a reference to the input
// text. It does not copy it.
void Lexer_SetInput(LexerRef _Nonnull self, const char* _Nullable source)
{
    self->source = (source) ? source : "";
    self->sourceIndex = 0;

    // Get the first token
    Lexer_ConsumeToken(self);
}

static void Lexer_AddCharToTextBuffer(LexerRef _Nonnull self, char ch)
{
    if (self->textBufferCount == self->textBufferCapacity) {
        int newCapacity = (self->textBufferCapacity > 0) ? self->textBufferCapacity * 2 : INITIAL_TEXT_BUFFER_CAPACITY;
        char* pNewTextBuffer = (char*) realloc(self->textBuffer, newCapacity);

        assert(pNewTextBuffer != NULL);
        self->textBuffer = pNewTextBuffer;
        self->textBufferCapacity = newCapacity;
    }

    self->textBuffer[self->textBufferCount++] = ch;
}

// Scans a variable name. Expects that the current input position is at the first
// character of the variable name.
static void Lexer_ScanVariableName(LexerRef _Nonnull self)
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
    self->textBufferCount--;
}

// Scans a single quoted string. Expects that the current input position is at
// the first character of the string contents.
static void Lexer_ScanSingleQuotedString(LexerRef _Nonnull self)
{
    self->textBufferCount = 0;

    while (true) {
        const char ch = self->source[self->sourceIndex];

        if (ch == '\0') {
            printf("Error: unexpected end of string\n");
            break;
        }

        self->sourceIndex++;
        self->column++;

        if (ch == '\'') {
            break;
        }
        Lexer_AddCharToTextBuffer(self, ch);
    }

    Lexer_AddCharToTextBuffer(self, '\0');
    self->textBufferCount--;
}

// Scans an octal code escape sequence of one, two or three digits into the text
// buffer at its current position. Expects that the current input position is at
// the first (valid) digit
static void Lexer_ScanOctalEscapeSequence(LexerRef _Nonnull self)
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
static void Lexer_ScanHexByteEscapeSequence(LexerRef _Nonnull self)
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

// Scans an escape sequence that appears inside of a double quoted string. Expects
// that the current input position is at the first character following the
// initial '\' character.
static void Lexer_ScanEscapeSequence(LexerRef _Nonnull self)
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
            return;

        case 'x':
        case 'X':
            self->sourceIndex++;
            self->column++;
            Lexer_ScanHexByteEscapeSequence(self);
            return;
            
        // XXX add \uxxxx and \Uxxxxyyyy (Unicode) support

        case '\0':
            printf("Error: incomplete escape sequence\n");
            return;

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
            printf("Error: unexpected escape sequence (ignored)\n");
            self->sourceIndex++;
            self->column++;
            return;
    }

    self->sourceIndex++;
    Lexer_AddCharToTextBuffer(self, ch);
}

// Scans a double quoted string. Expects that the current input position is at
// the first character of the string contents.
static void Lexer_ScanDoubleQuotedString(LexerRef _Nonnull self)
{
    bool done = false;

    self->textBufferCount = 0;

    while (!done) {
        const char ch = self->source[self->sourceIndex];

        if (ch == '\0') {
            printf("Error: unexpected end of string\n");
            break;
        }

        self->sourceIndex++;
        self->column++;

        switch(ch) {
            case '"':
                done = true;
                break;

            case '\\':
                Lexer_ScanEscapeSequence(self);
                break;

            default:
                Lexer_AddCharToTextBuffer(self, ch);
                break;
        }
    }

    Lexer_AddCharToTextBuffer(self, '\0');
    self->textBufferCount--;
}

// Scans a quoted character. Expects that the current input position is at the
// first character following the initial '\' character.
static void Lexer_ScanQuotedCharacter(LexerRef _Nonnull self)
{
    char ch = self->source[self->sourceIndex];

    switch (ch) {
        case '\0':
            printf("Error: incomplete escape sequence\n");
            return;

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
            // Our caller expects a single \n character
            break;

        default:
            break;
    }

    self->sourceIndex++;
    Lexer_AddCharToTextBuffer(self, ch);
}

// Returns true if the given character is a valid morpheme character; false otherwise.
// Characters which are not valid word characters are used to separate words.
static bool isMorphemeChar(char ch)
{
    switch (ch) {
        case '\0':
            return false;

        case '(':
        case ')':
        case '|':
        case '<':
        case '>':
        case '&':
        case '#':
        case ';':
        case '$':
        case '\"':
        case '\'':
        case '\\':
            return false;

        default:
            return (isgraph(ch) != 0) ? true : false;
    }
}

// Scans a morpheme. Expects that the current input position is at the first
// character of the morpheme.
static void Lexer_ScanMorpheme(LexerRef _Nonnull self)
{
    self->textBufferCount = 0;

    while (true) {
        const char ch = self->source[self->sourceIndex];

        if (!isMorphemeChar(ch)) {
            break;
        }

        self->sourceIndex++;
        self->column++;
        Lexer_AddCharToTextBuffer(self, ch);
    }

    Lexer_AddCharToTextBuffer(self, '\0');
    self->textBufferCount--;
}

static void Lexer_SkipWhitespace(LexerRef _Nonnull self)
{
    while (true) {
        const char ch = self->source[self->sourceIndex];

        if (ch != ' ' && ch != '\t' && ch != '\v' && ch != '\f') {
            break;
        }

        self->sourceIndex++;
        self->column++;
    }
}

static void Lexer_SkipEndOfLineComment(LexerRef _Nonnull self)
{
    while (true) {
        const char ch = self->source[self->sourceIndex];

        if (ch == '\n' || (ch == '\r' && self->source[self->sourceIndex + 1] == '\n')) {
            break;
        }

        self->sourceIndex++;
        self->column++;
    }
}

static bool hasTrailingWhitespace(LexerRef _Nonnull self)
{
    const char ch = self->source[self->sourceIndex];
    return (ch == '\0' || isspace(ch));
}

void Lexer_ConsumeToken(LexerRef _Nonnull self)
{
    self->t.column = self->column;
    self->t.line = self->line;
    self->t.length = 0;
    self->t.u.string = NULL;
    
    while (true) {
        const char ch = self->source[self->sourceIndex];

        switch (ch) {
            case '\0':
                self->t.id = kToken_Eof;
                self->t.hasTrailingWhitespace = true;
                return;

            case ' ':
            case '\t':
            case '\v':
            case '\f':
                Lexer_SkipWhitespace(self);
                break;

            case '#':
                Lexer_SkipEndOfLineComment(self);
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
                self->t.hasTrailingWhitespace = hasTrailingWhitespace(self);
                return;

            case '(':
            case ')':
            case '<':
            case '>':
            case '|':
            case '&':
            case ';':
                self->sourceIndex++;
                self->column++;

                self->t.id = ch;
                self->t.hasTrailingWhitespace = hasTrailingWhitespace(self);
                return;

            case '$':
                self->sourceIndex++;
                self->column++;
                Lexer_ScanVariableName(self);

                self->t.id = kToken_VariableName;
                self->t.u.string = self->textBuffer;
                self->t.length = self->textBufferCount;
                self->t.hasTrailingWhitespace = hasTrailingWhitespace(self);
                return;

            case '\'':
                self->sourceIndex++;
                self->column++;
                Lexer_ScanSingleQuotedString(self);

                self->t.id = kToken_SingleQuotedString;
                self->t.u.string = self->textBuffer;
                self->t.length = self->textBufferCount;
                self->t.hasTrailingWhitespace = hasTrailingWhitespace(self);
                return;

            case '"':
                self->sourceIndex++;
                self->column++;
                Lexer_ScanDoubleQuotedString(self);

                self->t.id = kToken_DoubleQuotedString;
                self->t.u.string = self->textBuffer;
                self->t.length = self->textBufferCount;
                self->t.hasTrailingWhitespace = hasTrailingWhitespace(self);
                return;

            case '\\':
                self->sourceIndex++;
                self->column++;
                self->textBufferCount = 0;
                Lexer_ScanQuotedCharacter(self);
                Lexer_AddCharToTextBuffer(self, '\0');
                self->textBufferCount--;

                if (self->textBufferCount == 1 && self->textBuffer[0] == '\n') {
                    // a line continuation escape
                    break;
                }
                else {
                    self->t.id = kToken_QuotedCharacter;
                    self->t.u.string = self->textBuffer;
                    self->t.length = self->textBufferCount;
                    self->t.hasTrailingWhitespace = hasTrailingWhitespace(self);
                    return;
                }

            default:
                if (isMorphemeChar(ch)) {
                    Lexer_ScanMorpheme(self);

                    self->t.id = kToken_UnquotedString;
                    self->t.u.string = self->textBuffer;
                    self->t.length = self->textBufferCount;
                }
                else {
                    self->sourceIndex++;
                    self->column++;

                    self->t.id = kToken_Character;
                    self->t.u.character = ch;
                    self->t.length = 1;
                }
                self->t.hasTrailingWhitespace = hasTrailingWhitespace(self);
                return;
        }
    }
}

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

#define INITIAL_WORD_BUFFER_CAPACITY    16


errno_t Lexer_Init(LexerRef _Nonnull self)
{
    memset(self, 0, sizeof(Lexer));

    self->text = "";
    self->textIndex = 0;
    self->wordBufferCapacity = 0;
    self->wordBufferCount = 0;
    
    return 0;
}

void Lexer_Deinit(LexerRef _Nonnull self)
{
    self->text = NULL;
    free(self->wordBuffer);
    self->wordBuffer = NULL;
}

// Sets the lexer input. Note that the lexer maintains a reference to the input
// text. It does not copy it.
void Lexer_SetInput(LexerRef _Nonnull self, const char* _Nullable text)
{
    self->text = (text) ? text : "";
    self->textIndex = 0;

    // Get the first token
    Lexer_ConsumeToken(self);
}

static void Lexer_AddCharToWordBuffer(LexerRef _Nonnull self, char ch)
{
    if (self->wordBufferCount == self->wordBufferCapacity) {
        int newCapacity = (self->wordBufferCapacity > 0) ? self->wordBufferCapacity * 2 : INITIAL_WORD_BUFFER_CAPACITY;
        char* pNewWordBuffer = (char*) realloc(self->wordBuffer, newCapacity);

        assert(pNewWordBuffer != NULL);
        self->wordBuffer = pNewWordBuffer;
        self->wordBufferCapacity = newCapacity;
    }

    self->wordBuffer[self->wordBufferCount++] = ch;
}

// Scans a single quoted string. Expects that the current input position is at
// the first character of the string contents.
static void Lexer_ScanSingleQuotedString(LexerRef _Nonnull self)
{
    self->wordBufferCount = 0;

    while (true) {
        const char ch = self->text[self->textIndex];

        if (ch == '\0') {
            printf("Error: unexpected end of string\n");
            break;
        }

        self->textIndex++;

        if (ch == '\'') {
            break;
        }
        Lexer_AddCharToWordBuffer(self, ch);
    }

    Lexer_AddCharToWordBuffer(self, '\0');
    self->wordBufferCount--;
}

// Scans an octal code escape sequence of one, two or three digits. Expects that
// the current input position is at the first (valid) digit
static void Lexer_ScanOctalEscapeSequence(LexerRef _Nonnull self)
{
    int val = 0;

    for (int i = 0; i < 3; i++) {
        char ch = self->text[self->textIndex];

        if (ch < '0' || ch > '7') {
            break;
        }

        self->textIndex++;
        val = (val << 3) + (ch - '0');
    }

    Lexer_AddCharToWordBuffer(self, val & 0xff);
}

// Scans a single byte escape code in the form of a hexadecimal number. Expects
// that the current input position is at the first (valid) digit.
static void Lexer_ScanHexByteEscapeSequence(LexerRef _Nonnull self)
{
    int val = 0;

    for (int i = 0; i < 2; i++) {
        char ch = self->text[self->textIndex];

        if (!isxdigit(ch)) {
            break;
        }

        self->textIndex++;
        const int d = (ch >= 'a') ? ch - 'a' : ((ch >= 'A') ? ch - 'A' : ch - '0');
        val = (val << 4) + d;
    }

    Lexer_AddCharToWordBuffer(self, val & 0xff);
}

// Scans an escape sequence. Expects that the current input position is at the
// first character following the initial '\' character.
static void Lexer_ScanEscapeSequence(LexerRef _Nonnull self)
{
    char ch = self->text[self->textIndex];

    switch (ch) {
        case 'a':   ch = 0x07;  break;
        case 'b':   ch = 0x08;  break;
        case 'e':   ch = 0x1b;  break;
        case 'f':   ch = 0x0c;  break;
        case 'r':   ch = 0x0d;  break;
        case 'v':   ch = 0x0b;  break;

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
            self->textIndex++;
            Lexer_ScanHexByteEscapeSequence(self);
            return;
            
        // XXX add \uxxxx and \Uxxxxyyyy (Unicode) support
        
        case '\0':
            return;

        default:
            printf("Error: unexpected escape sequence (ignored)\n");
            self->textIndex++;
            return;
    }

    self->textIndex++;
    Lexer_AddCharToWordBuffer(self, ch);
}

// Scans a double quoted string. Expects that the current input position is at
// the first character of the string contents.
static void Lexer_ScanDoubleQuotedString(LexerRef _Nonnull self)
{
    bool done = false;

    self->wordBufferCount = 0;

    while (!done) {
        const char ch = self->text[self->textIndex];

        if (ch == '\0') {
            printf("Error: unexpected end of string\n");
            break;
        }

        self->textIndex++;

        switch(ch) {
            case '"':
                done = true;
                break;

            case '\\':
                Lexer_ScanEscapeSequence(self);
                break;

            default:
                Lexer_AddCharToWordBuffer(self, ch);
                break;
        }
    }

    Lexer_AddCharToWordBuffer(self, '\0');
    self->wordBufferCount--;
}

// Scans a word. Expects that the current input position is at the first character
// of the word.
static void Lexer_ScanWord(LexerRef _Nonnull self)
{
    self->wordBufferCount = 0;

    while (true) {
        const char ch = self->text[self->textIndex];

        if (ch == '\0' || ch == '#' || ch == ';' || !isgraph(ch)) {
            break;
        }

        self->textIndex++;
        Lexer_AddCharToWordBuffer(self, ch);
    }

    Lexer_AddCharToWordBuffer(self, '\0');
    self->wordBufferCount--;
}

void Lexer_ConsumeToken(LexerRef _Nonnull self)
{
    while (true) {
        const char ch = self->text[self->textIndex];

        switch (ch) {
            case '\0':
                self->t.id = kToken_Eof;
                return;

            case '\n':
            case '\r':
            case ';':
                self->t.id = kToken_Eos;
                self->textIndex++;
                return;

            case ' ':
            case '\t':
            case '\v':
            case '\f':
                while(isblank(self->text[self->textIndex])) {
                    self->textIndex++;
                }
                break;

            case '#':
                self->textIndex++;
                while(self->text[self->textIndex] != '\n') {
                    self->textIndex++;
                }
                break;

            case '\'':
                self->textIndex++;
                Lexer_ScanSingleQuotedString(self);
                self->t.id = kToken_Word;
                self->t.u.word.text = self->wordBuffer;
                self->t.u.word.length = self->wordBufferCount;
                return;

            case '"':
                self->textIndex++;
                Lexer_ScanDoubleQuotedString(self);
                self->t.id = kToken_Word;
                self->t.u.word.text = self->wordBuffer;
                self->t.u.word.length = self->wordBufferCount;
                return;

            default:
                if (isgraph(ch)) {
                    Lexer_ScanWord(self);
                    self->t.id = kToken_Word;
                    self->t.u.word.text = self->wordBuffer;
                    self->t.u.word.length = self->wordBufferCount;
                }
                else {
                    self->t.id = kToken_Character;
                    self->t.u.character = ch;
                    self->textIndex++;
                }
                return;
        }
    }
}

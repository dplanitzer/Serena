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

        if (ch == '\'') {
            self->textIndex++;
            break;
        }
        else if (ch == '\0') {
            printf("Error: unexpected end of string\n");
            break;
        }

        self->textIndex++;
        Lexer_AddCharToWordBuffer(self, ch);
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

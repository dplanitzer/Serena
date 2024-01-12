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

#define INITIAL_WORD_BUFFER_CAPACITY    16


errno_t Lexer_Init(LexerRef _Nonnull self)
{
    memset(self, 0, sizeof(Lexer));

    self->text = "";
    self->textIndex = 0;

    self->wordBuffer = (char*)malloc(INITIAL_WORD_BUFFER_CAPACITY + 1);
    if (self->wordBuffer == NULL) {
        Lexer_Deinit(self);
        return ENOMEM;
    }
    self->wordBufferCapacity = INITIAL_WORD_BUFFER_CAPACITY;
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

static void Lexer_ScanWord(LexerRef _Nonnull self)
{
    self->wordBufferCount = 0;

    while (true) {
        const char ch = self->text[self->textIndex++];

        if (ch == '\0' || ch == '#' || !isgraph(ch)) {
            break;
        }

        if (self->wordBufferCount == self->wordBufferCapacity) {
            int newCapacity = self->wordBufferCapacity * 2;
            char* pNewWordBuffer = (char*) realloc(self->wordBuffer, newCapacity + 1);

            assert(pNewWordBuffer != NULL);
            self->wordBuffer = pNewWordBuffer;
            self->wordBufferCapacity = newCapacity;
        }

        self->wordBuffer[self->wordBufferCount++] = ch;
    }

    self->textIndex--;
    self->wordBuffer[self->wordBufferCount] = '\0';
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

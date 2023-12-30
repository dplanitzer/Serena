//
//  LineReader.c
//  sh
//
//  Created by Dietmar Planitzer on 12/29/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "LineReader.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


errno_t LineReader_Create(int maxLineLength, LineReaderRef _Nullable * _Nonnull pOutReader)
{
    LineReaderRef self = (LineReaderRef)malloc(sizeof(LineReader) + maxLineLength);

    if (self) {
        self->lineCapacity = maxLineLength + 1;
        self->lineCount = 0;
        self->x = 0;
        self->maxX = maxLineLength - 1;
        self->line[0] = '\0';

        *pOutReader = self;
        return 0;
    }

    *pOutReader = NULL;
    return ENOMEM;
}

void LineReader_Destroy(LineReaderRef _Nullable self)
{
    if (self) {
        free(self);
    }
}

static void LineReader_MoveCursorLeft(LineReaderRef _Nonnull self)
{
    if (self->x > 0) {
        printf("\033[D");   // cursor left
        self->x--;
    }
}

static void LineReader_MoveCursorRight(LineReaderRef _Nonnull self)
{
    if (self->x < self->maxX) {
        printf("\033[C");   // cursor right
        self->x++;
    }
}

static void LineReader_DeleteCharacter(LineReaderRef _Nonnull self)
{
    if (self->x == 0) {
        return;
    }

    for(int i = self->x; i < self->lineCount; i++) {
        self->line[i - 1] = self->line[i];
    }
    self->x--;
    self->lineCount--;

    putchar(8);
}

// XXX Replace this with a proper ESC sequence parser
static void LineReader_ReadEscapeSequence(LineReaderRef _Nonnull self)
{
    const char lbracket = getchar();    // [
    const char dir = getchar();         // cursor direction

    switch (dir) {
        case 'A':
            // Up
            break;

        case 'B':
            // Down
            break;

        case 'C':
            // Right
            LineReader_MoveCursorRight(self);
            break;

        case 'D':
            // Left
            LineReader_MoveCursorLeft(self);
            break;

        default:
            // Ignore for now
            break;
    }
}

static void LineReader_AcceptCharacter(LineReaderRef _Nonnull self, int ch)
{
    self->line[self->x] = (char)ch;
    putchar(ch);

    if (self->x == self->maxX) {
        printf("\033[D");   // cursor left
    } else {
        self->x++;
    }

    self->lineCount++;
    if (self->lineCount > self->maxX + 1) {
        self->lineCount = self->maxX + 1;
    }
}

char* _Nonnull LineReader_ReadLine(LineReaderRef _Nonnull self)
{
    bool done = false;

    self->line[0] = '\0';
    self->lineCount = 0;
    self->x = 0;

    while (!done) {
        const int ch = getchar();

        switch (ch) {
            case '\n':
                done = true;
                break;

            case 8:
                LineReader_DeleteCharacter(self);
                break;

            case 033:
                LineReader_ReadEscapeSequence(self);
                break;

            default:
                LineReader_AcceptCharacter(self, ch);
                break;
        }
    }

    self->line[self->lineCount] = '\0';
    return self->line;
}

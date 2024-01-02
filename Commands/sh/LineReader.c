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


errno_t LineReader_Create(int maxLineLength, int historyCapacity, const char* _Nonnull pPrompt, LineReaderRef _Nullable * _Nonnull pOutReader)
{
    LineReaderRef self = (LineReaderRef)calloc(1, sizeof(LineReader) + maxLineLength);

    if (self) {
        self->lineCapacity = maxLineLength + 1;
        self->lineCount = 0;
        self->x = 0;
        self->maxX = maxLineLength - 1;
        self->savedLine = NULL;
        self->isDirty = false;
        self->line[0] = '\0';

        self->history = (char**)calloc(historyCapacity, sizeof(char*));
        self->historyCapacity = historyCapacity;
        self->historyCount = 0;
        self->historyIndex = 0;

        if (historyCapacity > 0 && self->history == NULL) {
            LineReader_Destroy(self);
            return ENOMEM;
        }

        self->prompt = strdup(pPrompt);
        if (self == NULL) {
            LineReader_Destroy(self);
            return ENOMEM;
        }

        *pOutReader = self;
        return 0;
    }

    *pOutReader = NULL;
    return ENOMEM;
}

void LineReader_Destroy(LineReaderRef _Nullable self)
{
    if (self) {
        if (self->history) {
            for (int i = 0; i < self->historyCount; i++) {
                free(self->history[i]);
                self->history[i] = NULL;
            }
            free(self->history);
            self->history = NULL;
        }
        
        free(self->prompt);
        self->prompt = NULL;
        free(self->savedLine);
        self->savedLine = NULL;

        free(self);
    }
}

static void LineReader_PrintPrompt(LineReaderRef _Nonnull self)
{
    printf("%s", self->prompt);
}

static void LineReader_OnUserInput(LineReaderRef _Nonnull self)
{
    self->isDirty = true;
    self->historyIndex = self->historyCount;
}

static void LineReader_SaveLineIfDirty(LineReaderRef _Nonnull self)
{
    if (self->isDirty) {
        free(self->savedLine);
        self->savedLine = strdup(self->line);
        self->isDirty = false;
    }
}

// Does not mark the line reader input as dirty
static void LineReader_SetLine(LineReaderRef _Nonnull self, const char* _Nonnull pNewLine)
{
    strncpy(self->line, pNewLine, self->lineCapacity - 1);
    self->lineCount = strlen(pNewLine);
    if (self->lineCount >= self->lineCapacity) {
        self->lineCount = self->lineCapacity - 2;
    }

    // Move the cursor to the character after the last character of the new line
    self->x = self->lineCount;
    if (self->x > self->maxX) {
        self->x = self->maxX;
    }

    printf("\033[2K\r");
    LineReader_PrintPrompt(self);
    printf("%s", self->line);
}

static void LineReader_PrintHistory(LineReaderRef _Nonnull self, const char* _Nonnull info)
{
    printf("\nafter %s:\n", info);
    if (self->historyCount > 0) {
        for (int i = self->historyCount - 1; i >= 0; i--) {
            printf("%d:  \"%s\"\n", i, self->history[i]);
        }
    } else {
        printf("  <empty>\n");
    }
    printf("sel idx: %d\n", self->historyIndex);
}

static void LineReader_PushHistory(LineReaderRef _Nonnull self, char* _Nonnull pLine)
{
    if (self->historyCapacity == 0) {
        return;
    }


    // Only add 'pLine' to the history if it is different from what's currently
    // on top of the history stack
    if (self->historyCount > 0) {
        if (!strcmp(self->history[self->historyCount - 1], pLine)) {
            return;
        }
    }


    // Add 'pLine' to the history. It replaces the oldest entry if the history
    // is at capacity.
    if (self->historyCount == self->historyCapacity) {
        free(self->history[0]);

        for (int i = 1; i < self->historyCount; i++) {
            self->history[i - 1] = self->history[i];
        }

        self->historyCount--;
    }

    self->history[self->historyCount] = strdup(pLine);
    if (self->history[self->historyCount]) {
        self->historyCount++;
    }
}

static void LineReader_MoveHistoryUp(LineReaderRef _Nonnull self)
{
    if (self->historyCount == 0 || self->historyIndex < 1) {
        return;
    }

    LineReader_SaveLineIfDirty(self);

    self->historyIndex--;
    LineReader_SetLine(self, self->history[self->historyIndex]);
}

static void LineReader_MoveHistoryDown(LineReaderRef _Nonnull self)
{
    if (self->historyCount == 0 || self->historyIndex == self->historyCount) {
        return;
    }

    self->historyIndex++;
    if (self->historyIndex < self->historyCount) {
        LineReader_SetLine(self, self->history[self->historyIndex]);
    } else {
        LineReader_SetLine(self, self->savedLine);
        free(self->savedLine);
        self->savedLine = NULL;
    }
}

static void LineReader_MoveCursorLeft(LineReaderRef _Nonnull self)
{
    if (self->x > 0) {
        printf("\033[D");   // cursor left
        self->x--;
        LineReader_OnUserInput(self);
    }
}

static void LineReader_MoveCursorRight(LineReaderRef _Nonnull self)
{
    if (self->x < self->maxX) {
        printf("\033[C");   // cursor right
        self->x++;
        LineReader_OnUserInput(self);
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
    LineReader_OnUserInput(self);
}

// XXX Replace this with a proper ESC sequence parser
static void LineReader_ReadEscapeSequence(LineReaderRef _Nonnull self)
{
    const char lbracket = getchar();    // [
    const char dir = getchar();         // cursor direction

    switch (dir) {
        case 'A':
            // Up
            LineReader_MoveHistoryUp(self);
            break;

        case 'B':
            // Down
            LineReader_MoveHistoryDown(self);
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

    LineReader_OnUserInput(self);
}

char* _Nonnull LineReader_ReadLine(LineReaderRef _Nonnull self)
{
    bool done = false;

    LineReader_PrintPrompt(self);

    self->line[0] = '\0';
    self->lineCount = 0;
    self->x = 0;
    self->isDirty = false;
    self->historyIndex = self->historyCount;

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
    LineReader_PushHistory(self, self->line);

    return self->line;
}

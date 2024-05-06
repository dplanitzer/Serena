//
//  LineReader.c
//  sh
//
//  Created by Dietmar Planitzer on 12/29/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "LineReader.h"
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


errno_t LineReader_Create(int maxLineLength, int historyCapacity, const char* _Nonnull pPrompt, LineReaderRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    LineReaderRef self;
    
    try_null(self, calloc(1, sizeof(LineReader) + maxLineLength + 1), ENOMEM);
    self->lineCapacity = maxLineLength + 1;
    self->lineCount = 0;
    self->x = 0;
    self->maxX = maxLineLength - 1;
    self->savedLine = NULL;
    self->isDirty = false;
    self->line[0] = '\0';

    try_null(self->history, calloc(historyCapacity, sizeof(char*)), ENOMEM);
    self->historyCapacity = historyCapacity;
    self->historyCount = 0;
    self->historyIndex = 0;

    try_null(self->prompt, strdup(pPrompt), ENOMEM);

    // XXX not the best way or place to do it
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    // XXX
        
    *pOutSelf = self;
    return 0;

catch:
    LineReader_Destroy(self);
    *pOutSelf = NULL;
    return err;
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

static void LineReader_PrintInputLine(LineReaderRef _Nonnull self)
{
    self->line[self->lineCount] = '\0';
    printf("%s", self->line);
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

// Replaces the content of the input line with the given string and moves the
// text cursor after the last character in the line. Note that this function
// does not mark the line reader input as dirty.
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

// Returns the number of entries that currently exist in the history.
int LineReader_GetHistoryCount(LineReaderRef _Nonnull self)
{
    return self->historyCount;
}

// Returns a reference to the history entry at the given index. Entries are
// ordered ascending from oldest to newest.
const char* _Nonnull LineReader_GetHistoryAt(LineReaderRef _Nonnull self, int idx)
{
    return self->history[idx];
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

// Removes all entries in the history that exactly match 'pLine'. Returns true
// if at least one entry was removed from the stack and false otherwise.
static bool LineReader_RemoveFromHistory(LineReaderRef _Nonnull self, char* _Nonnull pLine)
{
    int nRemoved = 0;

    for (int i = 0; i < self->historyCount; i++) {
        if (!strcmp(pLine, self->history[i])) {
            free(self->history[i]);
            // 0 1 2 3 4 5 6 7
            for (int j = i + 1; j < self->historyCount; j++) {
                self->history[j - 1] = self->history[j];
            }
            self->history[self->historyCount - 1] = NULL;
            self->historyCount--;

            if (i <= self->historyIndex) {
                self->historyIndex--;
            }

            nRemoved++;
        }
    }

    return (nRemoved > 0) ? true : false;
}

static void LineReader_PushHistory(LineReaderRef _Nonnull self, char* _Nonnull pLine)
{
    if (self->historyCapacity == 0) {
        return;
    }


    // Only add 'pLine' if it isn't empty or purely whitespace
    bool isUseful = false;
    for (int i = 0; pLine[i] != '\0'; i++) {
        if (!isspace(pLine[i])) {
            isUseful = true;
            break;
        }
    }
    if (!isUseful) {
        return;
    }

    
    // Remove all existing occurrences 'pLine' from the history. Note that we'll
    // reset the historyIndex to the top of the stack if it turns out that we
    // effectively pulled the entry to which historyIndex pointed, to the top
    // of the history stack.
    const bool didPullUp = LineReader_RemoveFromHistory(self, pLine);


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

    if (didPullUp) {
        self->historyIndex = self->historyCount - 1;
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
    }
}

static void LineReader_MoveCursorRight(LineReaderRef _Nonnull self)
{
    if (self->x < self->maxX) {
        printf("\033[C");   // cursor right
        self->x++;
    }
}

static void LineReader_MoveCursorToBeginningOfLine(LineReaderRef _Nonnull self)
{
    printf("\015\033[%dC", (int)strlen(self->prompt));
    self->x = 0;
}

static void LineReader_MoveCursorToEndOfLine(LineReaderRef _Nonnull self)
{
    printf("\015\033[%dC", (int)strlen(self->prompt) + self->lineCount);
    self->x = self->lineCount;
}

static void LineReader_ClearScreen(LineReaderRef _Nonnull self)
{
    // Clear the screen but preserve the current state of the input line. This
    // action does not count as dirtying the input buffer.
    printf("\033[2J\033[H");
    LineReader_PrintPrompt(self);
    LineReader_PrintInputLine(self);
}

static void LineReader_Backspace(LineReaderRef _Nonnull self)
{
    if (self->x == 0) {
        return;
    }

    //XXX insert vs replace
    //for(int i = self->x; i < self->lineCount; i++) {
    //    self->line[i - 1] = self->line[i];
    //}
    self->x--;
    //XXXself->lineCount--;

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

    if (self->x < self->maxX) {
        putchar(ch);
        self->x++;
        self->lineCount++;
    }
    else {
        // replace mode, auto-wrap off, save cursor position, output character, restore cursor position, auto-wrap on, insertion mode
        // auto-wrap off: to stop the console from scrolling when we hit the bottom right screen corner
        //XXX not yet printf("\033[4l\033[?7l\0337%c\0338\033[?7h\033[4h", ch);
        printf("\033[?7l\0337%c\0338\033[?7h", ch);
    }

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

            case 1:
                LineReader_MoveCursorToBeginningOfLine(self);
                break;

            case 5:
                LineReader_MoveCursorToEndOfLine(self);
                break;

            case 8:
                LineReader_Backspace(self);
                break;

            case 12:
                LineReader_ClearScreen(self);
                break;

            case 033:
                LineReader_ReadEscapeSequence(self);
                break;

            case EOF:
                // XXX gotta decide what to do in this case
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

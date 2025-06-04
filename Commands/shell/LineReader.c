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
#include <errno.h>
#include <_math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/console.h>
#include <sys/ioctl.h>

enum {
    kChar_CursorUp = 0x01000000,
    kChar_CursorDown = 0x01000001,
    kChar_CursorLeft = 0x01000002,
    kChar_CursorRight = 0x01000003,
};


static void LineReader_DeleteHistory(LineReaderRef _Nonnull self);
static void LineReader_SaveLineIfDirty(LineReaderRef _Nonnull self);
static void LineReader_SetLine(LineReaderRef _Nonnull self, const char* _Nonnull pNewLine);
static void LineReader_PrintInputLine(LineReaderRef _Nonnull self);


LineReaderRef _Nonnull LineReader_Create(int x, int width)
{
    LineReaderRef self = calloc(1, sizeof(LineReader));

    self->fp_in = stdin;
    self->fp_out = stdout;

    self->lrX = x;
    self->lrWidth = width;

    self->prompt = malloc(8);
    self->prompt[0] = '\0';
    self->promptLength = 0;
    self->promptCapacity = 8;

    self->savedLine = NULL;
    self->isDirty = false;

    // XXX not the best way or place to do it
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    // XXX
    
    return self;
}

void LineReader_Destroy(LineReaderRef _Nullable self)
{
    if (self) {
        LineReader_DeleteHistory(self);
        
        free(self->prompt);
        self->prompt = NULL;
        free(self->line);
        self->line = NULL;
        free(self->savedLine);
        self->savedLine = NULL;

        self->fp_out = NULL;
        self->fp_in = NULL;

        free(self);
    }
}

////////////////////////////////////////////////////////////////////////////////

void LineReader_SetPrompt(LineReaderRef _Nonnull self, const char* _Nonnull str)
{
    size_t j = 0, len = strlen(str);

    self->promptLength = 0;
    self->prompt[0] = '\0';


    if ((len + 1) > self->promptCapacity) {
        self->prompt = realloc(self->prompt, len + 1);
        self->promptCapacity = len + 1;
    }
    

    for (size_t i = 0; i < len; i++) {
        if (isprint(str[i])) {
            self->prompt[j++] = str[i];
        }
    }

    self->prompt[j] = '\0';
    self->promptLength = j;
}


////////////////////////////////////////////////////////////////////////////////

static int tgetc(FILE* _Nonnull fp)
{
    const int ch = fgetc(fp);

    if (ch == 033) {
        const char lbracket = fgetc(fp);   // [
        const char dir = fgetc(fp);        // cursor direction

        switch (dir) {
            case 'A':   return kChar_CursorUp;
            case 'B':   return kChar_CursorDown;
            case 'C':   return kChar_CursorRight;
            case 'D':   return kChar_CursorLeft;
            default:    return 0;   // Ignore character
        }
    }
    else {
        return ch;
    }
}

static void twrite(FILE* _Nonnull fp, const char* _Nonnull str)
{
    fwrite(str, strlen(str), 1, fp);
}

static void tbs(FILE* _Nonnull fp)
{
    fputc(8, fp);
}

static void tmovetox(FILE* _Nonnull fp, int x)
{
    if (x > 0) {
        fprintf(fp, "\015\033[%dC", __abs(x));
    }
    else {
        fputc('\r', fp);
    }
}

static void tmovex(FILE* _Nonnull fp, int dir)
{
    if (dir < 0) {
        fwrite("\033[D", 3, 1, fp);   // cursor left
    }
    else if (dir > 0) {
        fwrite("\033[C", 3, 1, fp);   // cursor right
    }
}

static void tcls(FILE* _Nonnull fp)
{
    fwrite("\033[2J\033[H", 7, 1, fp);
}


////////////////////////////////////////////////////////////////////////////////

// Deletes all entries in the history
static void LineReader_DeleteHistory(LineReaderRef _Nonnull self)
{
    if (self->history) {
        for (int i = 0; i < self->historyCount; i++) {
            free(self->history[i]);
            self->history[i] = NULL;
        }

        free(self->history);
        self->history = NULL;
        self->historyCapacity = 0;
        self->historyCount = 0;
        self->historyIndex = 0;
    }
}

// Sets the history capacity. This is the maximum number of entries the history
// will keep. Note that changing the history capacity deletes whatever is
// currently stored in the history. The history capacity is 0 by default.
void LineReader_SetHistoryCapacity(LineReaderRef _Nonnull self, size_t capacity)
{
    LineReader_DeleteHistory(self);

    self->history = calloc(capacity, sizeof(char*));
    self->historyCapacity = capacity;
    self->historyCount = 0;
    self->historyIndex = 0;
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
    fprintf(self->fp_out, "\nafter %s:\n", info);
    if (self->historyCount > 0) {
        for (int i = self->historyCount - 1; i >= 0; i--) {
            printf("%d:  \"%s\"\n", i, self->history[i]);
        }
    } else {
        fprintf(self->fp_out, "  <empty>\n");
    }
    fprintf(self->fp_out, "sel idx: %d\n", self->historyIndex);
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


////////////////////////////////////////////////////////////////////////////////

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
static void LineReader_SetLine(LineReaderRef _Nonnull self, const char* _Nonnull new_line)
{
    memset(self->line, ' ', self->textLastCol + 1);
    self->textLastCol = -1;
    self->cursorX = 0;
    
    for (int i = 0; i <= self->lineLastCol && *new_line != '\0'; i++, new_line++) {
        self->line[i] = *new_line;
        self->textLastCol = i;
    }
    self->cursorX = __min(self->textLastCol + 1, self->lineLastCol);


    tmovetox(self->fp_out, self->inputAreaFirstCol);
    fwrite(self->line, self->lineLastCol + 1, 1, self->fp_out);
    tmovetox(self->fp_out, self->inputAreaFirstCol + self->cursorX);
}

static void LineReader_PrintPrompt(LineReaderRef _Nonnull self)
{
    if (self->promptWidth > 0) {
        tmovetox(self->fp_out, 0);
        fwrite(self->prompt, self->promptLength, 1, self->fp_out);
    }
}

static void LineReader_PrintInputLine(LineReaderRef _Nonnull self)
{
    if (self->textLastCol >= 0) {
        tmovetox(self->fp_out, self->inputAreaFirstCol);
        fwrite(self->line, self->textLastCol + 1, 1, self->fp_out);
    }
}


////////////////////////////////////////////////////////////////////////////////

static void LineReader_OnUserInput(LineReaderRef _Nonnull self)
{
    self->isDirty = true;
    self->historyIndex = self->historyCount;
}

static void LineReader_MoveCursorToBeginningOfLine(LineReaderRef _Nonnull self)
{
    self->cursorX = 0;
    tmovetox(self->fp_out, self->inputAreaFirstCol);
}

static void LineReader_MoveCursorToEndOfLine(LineReaderRef _Nonnull self)
{
    self->cursorX = __min(self->textLastCol + 1, self->lineLastCol);
    tmovetox(self->fp_out, self->inputAreaFirstCol + self->textLastCol + 1);
}

static void LineReader_MoveCursorLeft(LineReaderRef _Nonnull self)
{
    if (self->cursorX > 0) {
        self->cursorX--;
        tmovex(self->fp_out, -1);
    }
}

static void LineReader_MoveCursorRight(LineReaderRef _Nonnull self)
{
    if (self->cursorX <= self->textLastCol && self->cursorX < self->lineLastCol) {
        self->cursorX++;
        tmovex(self->fp_out, 1);
    }
}

static void LineReader_ClearScreen(LineReaderRef _Nonnull self)
{
    // Clear the screen but preserve the current state of the input line. This
    // action does not count as dirtying the input buffer.
    tcls(self->fp_out);
    LineReader_PrintPrompt(self);
    LineReader_PrintInputLine(self);
    tmovetox(self->fp_out, self->inputAreaFirstCol + self->cursorX);
}

static void LineReader_Backspace(LineReaderRef _Nonnull self)
{
    if (self->cursorX == 0 || self->textLastCol < 0) {
        return;
    }


    for (int i = self->cursorX; i <= self->textLastCol; i++) {
        self->line[i - 1] = self->line[i];
    }
    self->line[self->textLastCol] = ' ';

    self->cursorX--;
    self->textLastCol--;

    tbs(self->fp_out);
    fwrite(&self->line[self->cursorX], (self->textLastCol + 2) - self->cursorX, 1, self->fp_out);
    tmovetox(self->fp_out, self->inputAreaFirstCol + self->cursorX);

    LineReader_OnUserInput(self);
}

static void LineReader_InputCharacter(LineReaderRef _Nonnull self, int ch)
{
    self->line[self->cursorX] = ch;
    fputc(ch, self->fp_out);

    if (self->textLastCol < self->cursorX) {
        self->textLastCol = self->cursorX;
    }
    if (self->cursorX < self->lineLastCol) {
        self->cursorX++;
    }

    LineReader_OnUserInput(self);
}

static int LineReader_CalcLayout(LineReaderRef _Nonnull self)
{
    con_screen_t scr;
    con_cursor_t crs;

    ioctl(fileno(self->fp_out), kConsoleCommand_GetScreen, &scr);
    ioctl(fileno(self->fp_out), kConsoleCommand_GetCursor, &crs);

    self->lrY = crs.y - 1;
    self->promptX = self->lrX;
    self->promptWidth = self->promptLength;

    self->inputAreaFirstCol = self->promptX + self->promptWidth;

    const int lineLength = ((self->lrWidth >= 0) ? self->lrWidth : scr.columns) - self->promptWidth;
    const size_t lineCapacity = lineLength + 1;

    if (lineCapacity > 2048) {
        errno = EINVAL;
        return -1;
    }

    if (self->line) {
        if (self->lineCapacity < lineCapacity) {
            self->line = realloc(self->line, lineCapacity);
            self->lineCapacity = lineCapacity;
        }
    }
    else if (lineCapacity > 0) {
        self->line = malloc(lineCapacity);
        self->lineCapacity = lineCapacity;
    }
    else {
        free(self->line);
        self->line = NULL;
        self->lineCapacity = lineCapacity;
    }

    if (self->lineCapacity > 0) {
        memset(self->line, ' ', lineLength);
        self->line[lineLength] = '\0';
    }
    self->cursorX = 0;
    self->textLastCol = -1;
    self->lineLastCol = lineLength - 1;

    return 0;
}

char* _Nonnull LineReader_ReadLine(LineReaderRef _Nonnull self)
{
    if (LineReader_CalcLayout(self) < 0) {
        return "";
    }

    self->isDirty = false;
    self->historyIndex = self->historyCount;


    // Replace mode, auto-wrap off, cursor on, reset character attributes 
    twrite(self->fp_out, "\033[4l\033[?7l\033[?25h\033[0m");


    // Print the prompt
    LineReader_PrintPrompt(self);


    bool done = false;
    while (!done) {
        const int ch = tgetc(self->fp_in);

        switch (ch) {
            case '\n':
            case EOF:
                done = true;
                break;

            case 1:     // Ctrl-a
                LineReader_MoveCursorToBeginningOfLine(self);
                break;

            case 5:     // Ctrl-e
                LineReader_MoveCursorToEndOfLine(self);
                break;

            case 8:     // Backspace
                LineReader_Backspace(self);
                break;
                
            case 12:    // Ctrl-l
                LineReader_ClearScreen(self);
                break;

            case kChar_CursorLeft:
                LineReader_MoveCursorLeft(self);
                break;

            case kChar_CursorRight:
                LineReader_MoveCursorRight(self);
                break;

            case kChar_CursorUp:
                LineReader_MoveHistoryUp(self);
                break;

            case kChar_CursorDown:
                LineReader_MoveHistoryDown(self);
                break;

            default:
                if (isprint(ch)) {
                    LineReader_InputCharacter(self, ch);
                }
                break;
        }
    }


    // Replace mode, auto-wrap on, reset character attributes
    twrite(self->fp_out, "\033[4l\033[?7h\033[0m");

    self->line[self->textLastCol + 1] = '\0';
    LineReader_PushHistory(self, self->line);

    return self->line;
}

//
//  readline.c
//  readline
//
//  Created by Dietmar Planitzer on 12/29/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__readline.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <flowterm.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ext/math.h>
#include <serena/console.h>
#include <serena/fd.h>


static void rl_delete_history(rl_t _Nonnull self);
static void rl_set_max_history_count(rl_t _Nonnull self, size_t capacity);
static void rl_save_line_if_dirty(rl_t _Nonnull self);
static void rl_set_line(rl_t _Nonnull self, const char* _Nonnull pNewLine);
static void rl_print_input_line(rl_t _Nonnull self);


rl_t _Nullable rl_create(const rl_create_info_t* _Nonnull info)
{
    if (info->tag != RL_CREATE_STRUCT_TAG) {
        return NULL;
    }

    rl_t self = calloc(1, sizeof(struct readline));

    if (self) {
        self->lrX = info->x;
        self->lrWidth = info->width;

        self->savedLine = NULL;
        self->isDirty = false;

        self->flags.isInsertMode = 1;
        self->flags.hasTermInsertMode = 1;
    

        if (info->max_history_count > 0) {
            rl_set_max_history_count(self, info->max_history_count);
        }
        if (info->prompt && info->prompt[0] != '\0') {
            rl_set_prompt(self, info->prompt);
        }
    }

    return self;
}

void rl_destroy(rl_t _Nullable self)
{
    if (self) {
        rl_delete_history(self);
        
        free(self->prompt);
        self->prompt = NULL;
        free(self->line);
        self->line = NULL;
        free(self->savedLine);
        self->savedLine = NULL;

        free(self);
    }
}

////////////////////////////////////////////////////////////////////////////////

void rl_set_prompt(rl_t _Nonnull self, const char* _Nonnull str)
{
    char* np = strdup(str);

    if (np) {
        free(self->prompt);
        self->prompt = np;
        self->promptLength = strlen(str);
    }
}

////////////////////////////////////////////////////////////////////////////////

// Deletes all entries in the history
static void rl_delete_history(rl_t _Nonnull self)
{
    if (self->history) {
        for (size_t i = 0; i < self->historyCount; i++) {
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
static void rl_set_max_history_count(rl_t _Nonnull self, size_t capacity)
{
    rl_delete_history(self);

    self->history = calloc(capacity, sizeof(char*));
    self->historyCapacity = capacity;
    self->historyCount = 0;
    self->historyIndex = 0;
}

// Returns the number of entries that currently exist in the history.
size_t rl_history_count(rl_t _Nonnull self)
{
    return self->historyCount;
}

// Returns a reference to the history entry at the given index. Entries are
// ordered ascending from oldest to newest. Returns NULL if 'idx' is out of range.
const char* _Nullable rl_history_at(rl_t _Nonnull self, size_t idx)
{
    if (idx < self->historyCount) {
        return self->history[idx];
    }
    else {
        errno = EINVAL;
        return NULL;
    }
}

#if 0
static void rl_print_history(rl_t _Nonnull self, const char* _Nonnull info)
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
#endif

// Removes all entries in the history that exactly match 'pLine'. Returns true
// if at least one entry was removed from the stack and false otherwise.
static bool rl_remove_history(rl_t _Nonnull self, char* _Nonnull pLine)
{
    size_t nRemoved = 0;

    for (size_t i = 0; i < self->historyCount; i++) {
        if (!strcmp(pLine, self->history[i])) {
            free(self->history[i]);
            // 0 1 2 3 4 5 6 7
            for (size_t j = i + 1; j < self->historyCount; j++) {
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

static void rl_push_history(rl_t _Nonnull self, char* _Nonnull pLine)
{
    if (self->historyCapacity == 0) {
        return;
    }


    // Only add 'pLine' if it isn't empty or purely whitespace
    bool isUseful = false;
    for (size_t i = 0; pLine[i] != '\0'; i++) {
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
    const bool didPullUp = rl_remove_history(self, pLine);


    // Add 'pLine' to the history. It replaces the oldest entry if the history
    // is at capacity.
    if (self->historyCount == self->historyCapacity) {
        free(self->history[0]);

        for (size_t i = 1; i < self->historyCount; i++) {
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

static void rl_history_up(rl_t _Nonnull self)
{
    if (self->historyCount == 0 || self->historyIndex < 1) {
        return;
    }

    rl_save_line_if_dirty(self);

    self->historyIndex--;
    rl_set_line(self, self->history[self->historyIndex]);
}

static void rl_history_down(rl_t _Nonnull self)
{
    if (self->historyCount == 0 || self->historyIndex == self->historyCount) {
        return;
    }

    self->historyIndex++;
    if (self->historyIndex < self->historyCount) {
        rl_set_line(self, self->history[self->historyIndex]);
    } else {
        rl_set_line(self, self->savedLine);
        free(self->savedLine);
        self->savedLine = NULL;
    }
}


////////////////////////////////////////////////////////////////////////////////

static void rl_save_line_if_dirty(rl_t _Nonnull self)
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
static void rl_set_line(rl_t _Nonnull self, const char* _Nonnull new_line)
{
    memset(self->line, ' ', self->textLastCol + 1);
    self->textLastCol = -1;
    self->cursorX = 0;
    
    for (int i = 0; i <= self->lineLastCol && *new_line != '\0'; i++, new_line++) {
        self->line[i] = *new_line;
        self->textLastCol = i;
    }
    self->cursorX = __min(self->textLastCol + 1, self->lineLastCol);


    ft_cursor(FT_OFF);
    ft_moveto(self->inputAreaFirstCol + 1, self->lrY + 1);
    fwrite(self->line, 1, self->lineLastCol + 1, stdout);
    ft_moveto(self->inputAreaFirstCol + self->cursorX + 1, self->lrY + 1);
    ft_cursor(FT_ON);
}

static void rl_print_prompt(rl_t _Nonnull self)
{
    if (self->promptWidth > 0) {
        ft_moveto(self->promptX + 1, self->lrY + 1);
        fwrite(self->prompt, 1, self->promptLength, stdout);
    }
}

static void rl_print_input_line(rl_t _Nonnull self)
{
    if (self->textLastCol >= 0) {
        ft_moveto(self->inputAreaFirstCol + 1, self->lrY + 1);
        fwrite(self->line, 1, self->textLastCol + 1, stdout);
    }
}


////////////////////////////////////////////////////////////////////////////////

static void rl_on_user_input(rl_t _Nonnull self)
{
    self->isDirty = true;
    self->historyIndex = self->historyCount;
}

static void rl_cursor_bol(rl_t _Nonnull self)
{
    self->cursorX = 0;
    ft_moveto(self->inputAreaFirstCol + 1, self->lrY + 1);
}

static void rl_cursor_eol(rl_t _Nonnull self)
{
    self->cursorX = __min(self->textLastCol + 1, self->lineLastCol);
    ft_moveto(self->inputAreaFirstCol + self->textLastCol + 1 + 1, self->lrY + 1);}

static void rl_cursor_left(rl_t _Nonnull self)
{
    if (self->cursorX > 0) {
        self->cursorX--;
        ft_move(-1, 0);
    }
}

static void rl_cursor_right(rl_t _Nonnull self)
{
    if (self->cursorX <= self->textLastCol && self->cursorX < self->lineLastCol) {
        self->cursorX++;
        ft_move(1, 0);
    }
}

static void rl_cls(rl_t _Nonnull self)
{
    // Clear the screen but preserve the current state of the input line. This
    // action does not count as dirtying the input buffer.
    ft_cursor(FT_OFF);
    ft_cls();
    rl_print_prompt(self);
    rl_print_input_line(self);
    ft_moveto(self->inputAreaFirstCol + self->cursorX + 1, self->lrY + 1);
    ft_cursor(FT_ON);
}

static void rl_input_bs(rl_t _Nonnull self)
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

    ft_cursor(FT_OFF);
    putc(8, stdout);
    fwrite(&self->line[self->cursorX], 1, (self->textLastCol + 2) - self->cursorX, stdout);
    ft_moveto(self->inputAreaFirstCol + self->cursorX + 1, self->lrY + 1);
    ft_cursor(FT_ON);

    rl_on_user_input(self);
}

static void rl_input_del(rl_t _Nonnull self)
{
    if (self->cursorX > self->textLastCol) {
        return;
    }


    for (int i = self->cursorX + 1; i <= self->textLastCol; i++) {
        self->line[i - 1] = self->line[i];
    }
    self->line[self->textLastCol] = ' ';

    self->textLastCol--;

    ft_cursor(FT_OFF);
    fwrite(&self->line[self->cursorX], 1, (self->textLastCol + 2) - self->cursorX, stdout);
    ft_moveto(self->inputAreaFirstCol + self->cursorX + 1, self->lrY + 1);
    ft_cursor(FT_ON);

    rl_on_user_input(self);
}

static void rl_input_char(rl_t _Nonnull self, int ch)
{
    const int doInsert = self->flags.isInsertMode && self->cursorX < self->lineLastCol && self->cursorX <= self->textLastCol;

    if (doInsert) {
        for (int i = self->textLastCol; i >= self->cursorX; i--) {
            self->line[i + 1] = self->line[i];
        }
        self->line[self->cursorX] = ch;

        if (self->flags.hasTermInsertMode) {
            ft_insertmode(FT_ON);
            putc(self->line[self->cursorX], stdout);
            ft_insertmode(FT_OFF);
        }
        else {
            ft_cursor(FT_OFF);
            fwrite(&self->line[self->cursorX], 1, __min(self->textLastCol + 2, self->lineLastCol + 1) - self->cursorX, stdout);
            ft_moveto(self->inputAreaFirstCol + self->cursorX + 1 + 1, self->lrY + 1);
            ft_cursor(FT_ON);
        }

        if (self->textLastCol < self->lineLastCol) {
            self->textLastCol++;
        }
    }
    else {
        self->line[self->cursorX] = ch;

        putc(self->line[self->cursorX], stdout);

        if (self->textLastCol < self->cursorX) {
            self->textLastCol = self->cursorX;
        }
    }

    if (self->cursorX < self->lineLastCol) {
        self->cursorX++;
    }

    rl_on_user_input(self);
}

static int rl_layout(rl_t _Nonnull self)
{
    int x, y;
    int w, h;

    //XXX using these makes the line reader hang after doing a ft_cls() for some reason
//    ft_curpos(&x, &y);
//    ft_screensize(&w, &h);
    con_screen_t scr;
    con_cursor_t crs;

    fd_cntl(FD_STDOUT, IOCMD_TTY_SCREEN, &scr);
    fd_cntl(FD_STDOUT, IOCMD_TTY_CURSOR, &crs);
    x = crs.x;
    y = crs.y;
    w = scr.columns;
    h = scr.rows;

    self->lrY = y - 1;
    self->promptX = self->lrX;
    self->promptWidth = self->promptLength;

    self->inputAreaFirstCol = self->promptX + self->promptWidth;

    const int lineLength = ((self->lrWidth >= 0) ? self->lrWidth : w) - self->promptWidth;
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

const char* _Nonnull rl_readline(rl_t _Nonnull self)
{
    if (rl_layout(self) < 0) {
        return "";
    }

    self->isDirty = false;
    self->historyIndex = self->historyCount;


    // Replace mode, auto-wrap off, cursor on, reset character attributes
    ft_insertmode(FT_OFF);
    ft_autowrap(FT_OFF);
    ft_cursor(FT_ON);
    ft_style(FT_PLAIN);


    // Print the prompt
    rl_print_prompt(self);
    ft_flush();


    bool done = false;
    while (!done) {
        const int ch = ft_getchar(0);

        switch (ch) {
            case '\n':
            case EOF:
                done = true;
                break;

            case 1:     // Ctrl-a
            case FT_CHAR_HOME:
                rl_cursor_bol(self);
                break;

            case 5:     // Ctrl-e
            case FT_CHAR_END:
                rl_cursor_eol(self);
                break;

            case 8:     // Backspace
                rl_input_bs(self);
                break;
                
            case 12:    // Ctrl-l
                rl_cls(self);
                break;

            case 4:     // Ctrl-d
            case FT_CHAR_DELETE:
                rl_input_del(self);
                break;

            case 2:     // Ctrl-b
            case FT_CHAR_CURSOR_LEFT:
                rl_cursor_left(self);
                break;

            case 6:     // Ctrl-f
            case FT_CHAR_CURSOR_RIGHT:
                rl_cursor_right(self);
                break;

            case 9:     // Ctrl-i
            case FT_CHAR_INSERT:
                self->flags.isInsertMode = !self->flags.isInsertMode;
                break;

            case 16:    // Ctrl-p
            case FT_CHAR_CURSOR_UP:
                rl_history_up(self);
                break;

            case 14:    // Ctrl-n
            case FT_CHAR_CURSOR_DOWN:
                rl_history_down(self);
                break;

            default:
                if (isprint(ch)) {
                    rl_input_char(self, ch);
                }
                break;
        }

        ft_flush();
    }


    // Replace mode, auto-wrap on, reset character attributes
    ft_insertmode(FT_OFF);
    ft_autowrap(FT_ON);
    ft_style(FT_PLAIN);
    ft_flush();

    self->line[self->textLastCol + 1] = '\0';
    rl_push_history(self, self->line);

    return (const char*)self->line;
}

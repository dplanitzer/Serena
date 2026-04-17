//
//  table.c
//  status
//
//  Created by Dietmar Planitzer on 4/15/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ext/math.h>

table_t* _Nullable table_create(const table_column_t* _Nonnull tableColumns, int count)
{
    table_t* tp = calloc(1, sizeof(struct table));
    table_column_t* tcp = malloc(sizeof(struct table_column) * count);
    int widestColumnWidth = 0;

    if (tcp == NULL || tp == NULL) {
        table_free(tp);
        return NULL;
    }

    tp->tableColumn = tcp;
    tp->columnCount = count;
    tp->colSepChar = ' ';

    for (int c = 0; c < count; c++) {
        tcp[c].title = tableColumns[c].title;
        tcp[c].width = __min(__max(tableColumns[c].width, 0), 80);
        tcp[c].align = __min(__max(tableColumns[c].align, TABLE_ALIGN_LEFT), TABLE_ALIGN_RIGHT);

        if (tcp[c].width > widestColumnWidth) {
            widestColumnWidth = tcp[c].width;
        }
        
        tp->tableWidth += tcp[c].width;
        if (c < count - 1) {
            // column separator character
            tp->tableWidth += 1;
        }
    }

    tp->cellChars = malloc(widestColumnWidth + 1);
    if (tp->cellChars == NULL) {
        table_free(tp);
        return NULL;
    }

    return tp;
}

void table_free(table_t* _Nullable t)
{
    if (t) {
        free(t->tableColumn);
        free(t->cellChars);
        free(t);
    }
}

void table_set_cell_func(table_t* _Nonnull t, table_cell_func_t _Nullable f, void* _Nullable ctx)
{
    t->cellFunc = f;
    t->cellCtx = ctx;
}

void table_set_row_count(table_t* _Nonnull t, int count)
{
    t->rowCount = count;
}

void table_set_viewport(table_t* _Nonnull t, int yOrigin, int height)
{
    t->vpY = yOrigin;
    t->vpHeight = height;
}

void table_set_fill_viewport(table_t* _Nonnull t, bool flag)
{
    t->flags.fillViewport = flag;
}

static void _draw_cell(table_t* _Nonnull t, int col, const char* _Nonnull str)
{
    const size_t len = strlen(str);
    const size_t col_len = t->tableColumn[col].width;
    const size_t cont_len = __min(len, col_len);
    size_t start_idx, sep_len = 0;

    switch (t->tableColumn[col].align) {
        case TABLE_ALIGN_LEFT:
            start_idx = 0;
            break;

        case TABLE_ALIGN_CENTER:
            start_idx = (col_len - cont_len) / 2;
            break;

        case TABLE_ALIGN_RIGHT:
            start_idx = col_len - cont_len;
            break;
    }


    if (cont_len > 0) {
        if (start_idx > 0) {
            memset(t->cellChars, ' ', start_idx);
        }

        memcpy(&t->cellChars[start_idx], str, cont_len);

        if (start_idx + cont_len < col_len) {
            memset(&t->cellChars[start_idx + cont_len], ' ', col_len - (start_idx + cont_len));
        }
    }
    else {
        memset(t->cellChars, ' ', col_len);
    }
    if (col < t->columnCount - 1) {
        t->cellChars[col_len] = t->colSepChar;
        sep_len = 1;
    }


    fwrite(t->cellChars, col_len + sep_len, 1, stdout);
}

#define BLANK_LINE  0
#define DASH_LINE   1

void _draw_h_line(int width, int type)
{
#define N_DASHES    20
    static const char* g_dashes = "--------------------";
    static const char* g_blanks = "                    ";
    const char* lin_chars = (type == DASH_LINE) ? g_dashes : g_blanks;

    while (width >= N_DASHES) {
        fwrite(lin_chars, N_DASHES, 1, stdout);
        width -= N_DASHES;
    }
    if (width > 0) {
        fwrite(lin_chars, width, 1, stdout);
    }
}

void table_draw(table_t* _Nonnull t)
{
    if (t->columnCount == 0) {
        return;
    }


    // draw table header
    for (int col = 0; col < t->columnCount; col++) {
        _draw_cell(t, col, t->tableColumn[col].title);
    }
    fputc('\n', stdout);
    _draw_h_line(t->tableWidth, DASH_LINE);
    fputc('\n', stdout);


    // draw table rows
    for (int vpRow = 0; vpRow < t->vpHeight; vpRow++) {
        const int row = vpRow + t->vpY;

        if (!t->flags.fillViewport && row >= t->rowCount) {
            break;
        }

        if (row >= 0 && row < t->rowCount && t->cellFunc) {
            for (int col = 0; col < t->columnCount; col++) {
                _draw_cell(t, col, t->cellFunc(t->cellCtx, row, col));
            }
        }
        else {
            _draw_h_line(t->tableWidth, BLANK_LINE);
        }

        if (vpRow < t->vpHeight - 1) {
            // don't trigger scrolling in the last row
            fputc('\n', stdout);
        }
    }
}

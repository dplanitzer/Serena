//
//  status.c
//  cmds
//
//  Created by Dietmar Planitzer on 5/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef TABLE_H
#define TABLE_H 1

#include <_cmndef.h>
#include <stdbool.h>

typedef const char* _Nonnull (*table_cell_func_t)(void* ctx, int row, int col);

#define TABLE_ALIGN_LEFT    -1
#define TABLE_ALIGN_CENTER  0
#define TABLE_ALIGN_RIGHT   1

typedef struct table_column {
    const char* _Nonnull    title;
    int                     width;
    int                     align;
} table_column_t;

typedef struct table {
    table_column_t* _Nonnull    tableColumn;
    int                         columnCount;
    int                         rowCount;

    table_cell_func_t _Nullable cellFunc;
    void* _Nullable             cellCtx;

    char* _Nonnull              cellChars;  // Includes left/right padding and column separator character. Big enough for the widest column

    int                         tableWidth;
    int                         vpY;        // First visible table row
    int                         vpHeight;   // Number of visible table rows

    char                        colSepChar;

    struct __tabFlags {
        unsigned int    fillViewport:1;
        unsigned int    reserved:31;
    }                           flags;
} table_t;


extern table_t* _Nullable table_create(const table_column_t* _Nonnull tableColumns, int count);
extern void table_free(table_t* _Nullable t);

extern void table_set_cell_func(table_t* _Nonnull t, table_cell_func_t _Nullable f, void* _Nullable ctx);
extern void table_set_row_count(table_t* _Nonnull t, int count);

extern void table_set_viewport(table_t* _Nonnull t, int yOrigin, int height);
extern void table_set_fill_viewport(table_t* _Nonnull t, bool flag);

extern void table_draw(table_t* _Nonnull t);

#endif /* TABLE_H */
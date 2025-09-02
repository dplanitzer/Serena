//
//  ColorTable.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef ColorTable_h
#define ColorTable_h

#include "GObject.h"
#include <kern/errno.h>
#include <kpi/fb.h>


typedef struct CLUTEntry {
    uint8_t     r;
    uint8_t     g;
    uint8_t     b;
    uint8_t     flags;
} CLUTEntry;


typedef struct ColorTable {
    GObject     super;
    size_t      entryCount;
    uint16_t    entry[1];
} ColorTable;


extern errno_t ColorTable_Create(int id, size_t entryCount, ColorTable* _Nullable * _Nonnull pOutSelf);
extern void ColorTable_Destroy(ColorTable* _Nullable self);


extern errno_t ColorTable_SetEntry(ColorTable* _Nonnull self, size_t idx, RGBColor32 color);
extern errno_t ColorTable_SetEntries(ColorTable* _Nonnull self, size_t idx, size_t count, const RGBColor32* _Nonnull entries);

#endif /* ColorTable_h */

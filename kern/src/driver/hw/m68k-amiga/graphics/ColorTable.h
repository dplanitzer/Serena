//
//  ColorTable.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef ColorTable_h
#define ColorTable_h

#include <ext/try.h>
#include <ext/queue.h>
#include <kpi/framebuffer.h>


enum {
    kColorTableFlag_IsRegistered = 0x01,
};


typedef struct CLUTEntry {
    uint8_t     r;
    uint8_t     g;
    uint8_t     b;
    uint8_t     flags;
} CLUTEntry;


typedef struct ColorTable {
    deque_node_t    chain;
    int             id;
    int             refCount;
    uint16_t        flags;
    uint16_t        entryCount;
    uint16_t        entry[1];
} ColorTable;


extern errno_t ColorTable_Create(size_t entryCount, vio_rgb32_t defaultColor, ColorTable* _Nullable * _Nonnull pOutSelf);

#define ColorTable_AddRef(__self) \
(((ColorTable*)(__self))->refCount++)

extern void ColorTable_DelRef(ColorTable* _Nullable self);

extern ColorTable* _Nullable ColorTable_GetForId(int id);


#define ColorTable_GetId(__self) \
(((ColorTable*)(__self))->id)

extern errno_t ColorTable_SetEntry(ColorTable* _Nonnull self, size_t idx, vio_rgb32_t color);
extern errno_t ColorTable_SetEntries(ColorTable* _Nonnull self, size_t idx, size_t count, const vio_rgb32_t* _Nonnull entries);

#endif /* ColorTable_h */

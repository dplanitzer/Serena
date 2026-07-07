//
//  ColorTable.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ColorTable.h"
#include <kern/kalloc.h>

static int      g_next_clut_id = 1;
static deque_t  g_clut_table;


static uint16_t _convert_color(color_rgb32_t color)
{
    const uint16_t r = RGBColor32_GetRed(color);
    const uint16_t g = RGBColor32_GetGreen(color);
    const uint16_t b = RGBColor32_GetBlue(color);
    
    return (r >> 4 & 0x0f) << 8 | (g >> 4 & 0x0f) << 4 | (b >> 4 & 0x0f);
}

static void _destroy(ColorTable* _Nullable self)
{
    kfree(self);
}


errno_t ColorTable_Create(size_t entryCount, color_rgb32_t defaultColor, ColorTable* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ColorTable* self;
    
    switch (entryCount) {
        case 2:
        case 4:
        case 8:
        case 16:
        case 32:
//        case 64:
//        case 128:
//        case 256:
            break;

        default:
            return EINVAL;
    }

    try(kalloc(sizeof(ColorTable) + (entryCount - 1) * sizeof(uint16_t), (void**) &self));
    self->chain.next = NULL;
    self->chain.prev = NULL;
    self->id = g_next_clut_id++;
    self->refCount = 1;
    self->entryCount = entryCount;

    const uint16_t rgb444 = _convert_color(defaultColor);
    for (size_t i = 0; i < entryCount; i++) {
        self->entry[i] = rgb444;
    }
    

    deque_add_first(&g_clut_table, &self->chain);
    self->flags |= kColorTableFlag_IsRegistered;

catch:
    *pOutSelf = self;
    return err;
}

void ColorTable_DelRef(ColorTable* _Nullable self)
{
    if (self) {
        self->refCount--;
        if (self->refCount == 0) {
            _destroy(self);
        }
    }
}

ColorTable* _Nullable ColorTable_GetForId(int id)
{
    deque_for_each(&g_clut_table, ColorTable, it,
        if (it->id == id) {
            return it;
        }
    )
    return NULL;
}

// Writes the given RGB color to the color register at index idx
errno_t ColorTable_SetEntry(ColorTable* _Nonnull self, size_t idx, color_rgb32_t color)
{
    if (idx < self->entryCount) {
        self->entry[idx] = _convert_color(color);
        return EOK;
    }
    else {
        return EINVAL;
    }
}

errno_t ColorTable_SetEntries(ColorTable* _Nonnull self, size_t idx, size_t count, const color_rgb32_t* _Nonnull entries)
{
    if (idx + count > self->entryCount) {
        return EINVAL;
    }

    if (count > 0) {
        for (size_t i = 0; i < count; i++) {
            self->entry[idx + i] = _convert_color(entries[i]);
        }
    }

    return EOK;
}

//
//  ColorTable.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ColorTable.h"
#include <kern/kalloc.h>

static uint16_t _convert_color(RGBColor32 color);


errno_t ColorTable_Create(int id, size_t entryCount, RGBColor32 defaultColor, ColorTable* _Nullable * _Nonnull pOutSelf)
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
    self->super.chain.next = NULL;
    self->super.chain.prev = NULL;
    self->super.type = kGObject_ColorTable;
    self->super.id = id;
    self->super.refCount = 1;
    self->entryCount = entryCount;

    const uint16_t rgb444 = _convert_color(defaultColor);
    for (size_t i = 0; i < entryCount; i++) {
        self->entry[i] = rgb444;
    }

catch:
    *pOutSelf = self;
    return err;
}

void ColorTable_Destroy(ColorTable* _Nullable self)
{
    kfree(self);
}

static uint16_t _convert_color(RGBColor32 color)
{
    const uint16_t r = RGBColor32_GetRed(color);
    const uint16_t g = RGBColor32_GetGreen(color);
    const uint16_t b = RGBColor32_GetBlue(color);
    
    return (r >> 4 & 0x0f) << 8 | (g >> 4 & 0x0f) << 4 | (b >> 4 & 0x0f);
}

// Writes the given RGB color to the color register at index idx
errno_t ColorTable_SetEntry(ColorTable* _Nonnull self, size_t idx, RGBColor32 color)
{
    if (idx < self->entryCount) {
        self->entry[idx] = _convert_color(color);
        return EOK;
    }
    else {
        return EINVAL;
    }
}

errno_t ColorTable_SetEntries(ColorTable* _Nonnull self, size_t idx, size_t count, const RGBColor32* _Nonnull entries)
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

//
//  ColorTable.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ColorTable.h"
#include <kern/kalloc.h>


errno_t ColorTable_Create(int id, size_t entryCount, ColorTable* _Nullable * _Nonnull pOutSelf)
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

    try(kalloc_cleared(sizeof(ColorTable) + (entryCount - 1) * sizeof(uint16_t), (void**) &self));
    self->useCount = 0;
    self->id = id;
    self->entryCount = entryCount;

catch:
    *pOutSelf = self;
    return err;
}

void ColorTable_Destroy(ColorTable* _Nullable self)
{
    kfree(self);
}

// Writes the given RGB color to the color register at index idx
errno_t ColorTable_SetEntry(ColorTable* _Nonnull self, size_t idx, RGBColor32 color)
{
    decl_try_err();

    if (idx >= self->entryCount) {
        return EINVAL;
    }

    const uint16_t r = RGBColor32_GetRed(color);
    const uint16_t g = RGBColor32_GetGreen(color);
    const uint16_t b = RGBColor32_GetBlue(color);
    self->entry[idx] = (r >> 4 & 0x0f) << 8 | (g >> 4 & 0x0f) << 4 | (b >> 4 & 0x0f);

    return EOK;
}

errno_t ColorTable_SetEntries(ColorTable* _Nonnull self, size_t idx, size_t count, const RGBColor32* _Nonnull entries)
{
    if (idx + count > self->entryCount) {
        return EINVAL;
    }

    if (count > 0) {
        for (size_t i = 0; i < count; i++) {
            const RGBColor32 color = entries[i];
            const uint16_t r = RGBColor32_GetRed(color);
            const uint16_t g = RGBColor32_GetGreen(color);
            const uint16_t b = RGBColor32_GetBlue(color);
            
            self->entry[idx + i] = (r >> 4 & 0x0f) << 8 | (g >> 4 & 0x0f) << 4 | (b >> 4 & 0x0f);
        }
    }

    return EOK;
}

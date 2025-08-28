//
//  Screen.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Screen.h"
#include <kern/kalloc.h>
#include <machine/amiga/chipset.h>


// Creates a screen object.
// \param pConfig the video configuration
// \param pixelFormat the pixel format (must be supported by the config)
// \return the screen or null
errno_t Screen_Create(int id, const VideoConfiguration* _Nonnull vidCfg, Surface* _Nonnull srf, Screen* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Screen* self;
    
    try(kalloc_cleared(sizeof(Screen), (void**) &self));

    Surface_BeginUse(srf);
    self->id = id;
    self->surface = srf;
    self->vidConfig = *vidCfg;
    self->clutEntryCount = (int16_t)COLOR_COUNT;
    self->flags = kScreenFlag_IsNewCopperProgNeeded;    
    
    if (self->clutEntryCount > 0) {
        try(kalloc_cleared(sizeof(CLUTEntry) * self->clutEntryCount, (void**)&self->clut));
    }

    *pOutSelf = self;
    return EOK;
    
catch:
    Screen_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void Screen_Destroy(Screen* _Nullable self)
{
    if (self) {
        if (self->surface) {
            Surface_EndUse(self->surface);
            self->surface = NULL;
        }
        
        if (self->clut) {
            kfree(self->clut);
            self->clut = NULL;
        }

        kfree(self);
    }
}

void Screen_GetPixelSize(Screen* _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight)
{
    *pOutWidth = Surface_GetWidth(self->surface);
    *pOutHeight = Surface_GetHeight(self->surface);
}

// Writes the given RGB color to the color register at index idx
errno_t Screen_SetCLUTEntry(Screen* _Nonnull self, size_t idx, RGBColor32 color)
{
    decl_try_err();

    if (idx >= self->clutEntryCount) {
        return EINVAL;
    }

    CLUTEntry* ep = &self->clut[idx];
    ep->r = RGBColor32_GetRed(color);
    ep->g = RGBColor32_GetGreen(color);
    ep->b = RGBColor32_GetBlue(color);
    Screen_SetNeedsUpdate(self);

    return EOK;
}

// Sets the contents of 'count' consecutive CLUT entries starting at index 'idx'
// to the colors in the array 'entries'.
errno_t Screen_SetCLUTEntries(Screen* _Nonnull self, size_t idx, size_t count, const RGBColor32* _Nonnull entries)
{
    if (idx + count > self->clutEntryCount) {
        return EINVAL;
    }

    if (count > 0) {
        for (size_t i = 0; i < count; i++) {
            const RGBColor32 color = entries[i];
            CLUTEntry* ep = &self->clut[idx + i];

            ep->r = RGBColor32_GetRed(color);
            ep->g = RGBColor32_GetGreen(color);
            ep->b = RGBColor32_GetBlue(color);
            Screen_SetNeedsUpdate(self);
        }
    }

    return EOK;
}

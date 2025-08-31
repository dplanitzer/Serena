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
    try(ColorTable_Create(COLOR_COUNT, &self->clut));
    self->flags = kScreenFlag_IsNewCopperProgNeeded;    

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
        
        ColorTable_Destroy(self->clut);
        self->clut = NULL;

        kfree(self);
    }
}

void Screen_GetPixelSize(Screen* _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight)
{
    *pOutWidth = Surface_GetWidth(self->surface);
    *pOutHeight = Surface_GetHeight(self->surface);
}

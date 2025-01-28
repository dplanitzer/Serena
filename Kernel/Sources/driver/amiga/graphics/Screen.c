//
//  Screen.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Screen.h"


// Creates a screen object.
// \param pConfig the video configuration
// \param pixelFormat the pixel format (must be supported by the config)
// \return the screen or null
errno_t Screen_Create(const ScreenConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, Sprite* _Nonnull pNullSprite, Screen* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Screen* self;
    
    try(kalloc_cleared(sizeof(Screen), (void**) &self));
    
    self->screenConfig = pConfig;
    self->pixelFormat = pixelFormat;
    self->nullSprite = pNullSprite;
    self->flags.isInterlaced = ScreenConfiguration_IsInterlaced(pConfig);
    self->flags.isNewCopperProgNeeded = 1;
    self->clutCapacity = PixelFormat_GetCLUTEntryCount(pixelFormat);

    
    // Allocate an appropriate framebuffer
    try(Surface_Create(pConfig->width, pConfig->height, pixelFormat, &self->framebuffer));
    
    
    // Lock the new surface
    try(Surface_LockPixels(self->framebuffer, kSurfaceAccess_Read|kSurfaceAccess_ReadWrite));
    
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
        Surface_UnlockPixels(self->framebuffer);
        Surface_Destroy(self->framebuffer);
        self->framebuffer = NULL;
        
        kfree(self);
    }
}

errno_t Screen_AcquireSprite(Screen* _Nonnull self, const uint16_t* _Nonnull pPlanes[2], int x, int y, int width, int height, int priority, SpriteID* _Nonnull pOutSpriteId)
{
    decl_try_err();
    const ScreenConfiguration* pConfig = self->screenConfig;
    Sprite* pSprite;

    *pOutSpriteId = -1;

    if (width < 0 || width > MAX_SPRITE_WIDTH) {
        return E2BIG;
    }
    if (height < 0 || height > MAX_SPRITE_HEIGHT) {
        return E2BIG;
    }
    if (priority < 0 || priority >= NUM_HARDWARE_SPRITES) {
        return EINVAL;
    }
    if (self->sprite[priority]) {
        return EBUSY;
    }

    if ((err = Sprite_Create(pPlanes, height, &pSprite)) == EOK) {
        Sprite_SetPosition(pSprite, x, y, pConfig);

        self->sprite[priority] = pSprite;
        self->flags.isNewCopperProgNeeded = 1;
        *pOutSpriteId = priority;
    }

    return err;
}

// Relinquishes a hardware sprite
errno_t Screen_RelinquishSprite(Screen* _Nonnull self, SpriteID spriteId)
{
    decl_try_err();

    if (spriteId >= 0) {
        if (spriteId >= NUM_HARDWARE_SPRITES) {
            return EINVAL;
        }

        // XXX Sprite_Destroy(pScreen->sprite[spriteId]);
        // XXX actually free the old sprite instead of leaking it. Can't do this
        // XXX yet because we need to ensure that the DMA is no longer accessing
        // XXX the data before it freeing it.
        self->sprite[spriteId] = self->nullSprite;
        self->flags.isNewCopperProgNeeded = 1;
    }
    return EOK;
}

// Updates the position of a hardware sprite.
errno_t Screen_SetSpritePosition(Screen* _Nonnull self, SpriteID spriteId, int x, int y)
{
    decl_try_err();

    if (spriteId < 0 || spriteId >= NUM_HARDWARE_SPRITES) {
        return EINVAL;
    }

    Sprite_SetPosition(self->sprite[spriteId], x, y, self->screenConfig);
    return EOK;
}

// Updates the visibility of a hardware sprite.
errno_t Screen_SetSpriteVisible(Screen* _Nonnull self, SpriteID spriteId, bool isVisible)
{
    decl_try_err();

    if (spriteId < 0 || spriteId >= NUM_HARDWARE_SPRITES) {
        return EINVAL;
    }

    Sprite_SetVisible(self->sprite[spriteId], isVisible, self->screenConfig);
    return EOK;
}

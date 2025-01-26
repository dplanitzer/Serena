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
errno_t Screen_Create(const ScreenConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, Sprite* _Nonnull pNullSprite, Screen* _Nullable * _Nonnull pOutScreen)
{
    decl_try_err();
    Screen* pScreen;
    
    try(kalloc_cleared(sizeof(Screen), (void**) &pScreen));
    
    pScreen->screenConfig = pConfig;
    pScreen->pixelFormat = pixelFormat;
    pScreen->nullSprite = pNullSprite;
    pScreen->isInterlaced = ScreenConfiguration_IsInterlaced(pConfig);
    pScreen->clutCapacity = PixelFormat_GetCLUTCapacity(pixelFormat);

    
    // Allocate an appropriate framebuffer
    try(Surface_Create(pConfig->width, pConfig->height, pixelFormat, &pScreen->framebuffer));
    
    
    // Lock the new surface
    try(Surface_LockPixels(pScreen->framebuffer, kSurfaceAccess_Read|kSurfaceAccess_ReadWrite));
    
    *pOutScreen = pScreen;
    return EOK;
    
catch:
    Screen_Destroy(pScreen);
    *pOutScreen = NULL;
    return err;
}

void Screen_Destroy(Screen* _Nullable pScreen)
{
    if (pScreen) {
        Surface_UnlockPixels(pScreen->framebuffer);
        Surface_Destroy(pScreen->framebuffer);
        pScreen->framebuffer = NULL;
        
        kfree(pScreen);
    }
}

errno_t Screen_AcquireSprite(Screen* _Nonnull pScreen, const uint16_t* _Nonnull pPlanes[2], int x, int y, int width, int height, int priority, SpriteID* _Nonnull pOutSpriteId)
{
    decl_try_err();
    const ScreenConfiguration* pConfig = pScreen->screenConfig;
    Sprite* pSprite;

    if (width < 0 || width > MAX_SPRITE_WIDTH) {
        throw(E2BIG);
    }
    if (height < 0 || height > MAX_SPRITE_HEIGHT) {
        throw(E2BIG);
    }
    if (priority < 0 || priority >= NUM_HARDWARE_SPRITES) {
        throw(EINVAL);
    }
    if (pScreen->sprite[priority]) {
        throw(EBUSY);
    }

    try(Sprite_Create(pPlanes, height, &pSprite));
    Sprite_SetPosition(pSprite, x, y, pConfig);

    pScreen->sprite[priority] = pSprite;
    *pOutSpriteId = priority;

    return EOK;

catch:
    *pOutSpriteId = -1;
    return err;
}

// Relinquishes a hardware sprite
errno_t Screen_RelinquishSprite(Screen* _Nonnull pScreen, SpriteID spriteId)
{
    decl_try_err();

    if (spriteId >= 0) {
        if (spriteId >= NUM_HARDWARE_SPRITES) {
            throw(EINVAL);
        }

        // XXX Sprite_Destroy(pScreen->sprite[spriteId]);
        // XXX actually free the old sprite instead of leaking it. Can't do this
        // XXX yet because we need to ensure that the DMA is no longer accessing
        // XXX the data before it freeing it.
        pScreen->sprite[spriteId] = pScreen->nullSprite;
    }
    return EOK;

catch:
    return err;
}

// Updates the position of a hardware sprite.
errno_t Screen_SetSpritePosition(Screen* _Nonnull pScreen, SpriteID spriteId, int x, int y)
{
    decl_try_err();

    if (spriteId < 0 || spriteId >= NUM_HARDWARE_SPRITES) {
        throw(EINVAL);
    }

    Sprite_SetPosition(pScreen->sprite[spriteId], x, y, pScreen->screenConfig);
    return EOK;

catch:
    return err;
}

// Updates the visibility of a hardware sprite.
errno_t Screen_SetSpriteVisible(Screen* _Nonnull pScreen, SpriteID spriteId, bool isVisible)
{
    decl_try_err();

    if (spriteId < 0 || spriteId >= NUM_HARDWARE_SPRITES) {
        throw(EINVAL);
    }

    Sprite_SetVisible(pScreen->sprite[spriteId], isVisible, pScreen->screenConfig);
    return EOK;

catch:
    return err;
}

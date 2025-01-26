//
//  Screen.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Screen_h
#define Screen_h

#include <klib/List.h>
#include <klib/Types.h>
#include "PixelFormat.h"
#include "ScreenConfiguration.h"
#include "Sprite.h"
#include "Surface.h"


typedef struct Screen {
    Surface* _Nullable                  framebuffer;        // the screen framebuffer
    const ScreenConfiguration* _Nonnull screenConfig;
    PixelFormat                         pixelFormat;
    Sprite* _Nonnull                    nullSprite;
    Sprite* _Nonnull                    sprite[NUM_HARDWARE_SPRITES];
    bool                                isInterlaced;
    int16_t                             clutCapacity;       // how many entries the physical CLUT supports for this screen configuration
} Screen;

#define MAX_CLUT_ENTRIES    32


extern errno_t Screen_Create(const ScreenConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, Sprite* _Nonnull pNullSprite, Screen* _Nullable * _Nonnull pOutScreen);
extern void Screen_Destroy(Screen* _Nullable pScreen);

extern errno_t Screen_AcquireSprite(Screen* _Nonnull pScreen, const uint16_t* _Nonnull pPlanes[2], int x, int y, int width, int height, int priority, SpriteID* _Nonnull pOutSpriteId);
extern errno_t Screen_RelinquishSprite(Screen* _Nonnull pScreen, SpriteID spriteId);
extern errno_t Screen_SetSpritePosition(Screen* _Nonnull pScreen, SpriteID spriteId, int x, int y);
extern errno_t Screen_SetSpriteVisible(Screen* _Nonnull pScreen, SpriteID spriteId, bool isVisible);

#endif /* Screen_h */

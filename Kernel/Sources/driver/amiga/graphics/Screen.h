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
    int16_t                             clutCapacity;       // how many entries the physical CLUT supports for this screen configuration
    struct __Flags {
        unsigned int        isInterlaced:1;
        unsigned int        isNewCopperProgNeeded:1;
        unsigned int        reserved:30;
    }                                   flags;
} Screen;

#define MAX_CLUT_ENTRIES    32


extern errno_t Screen_Create(const ScreenConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, Sprite* _Nonnull pNullSprite, Screen* _Nullable * _Nonnull pOutSelf);
extern void Screen_Destroy(Screen* _Nullable pScreen);

extern errno_t Screen_AcquireSprite(Screen* _Nonnull self, const uint16_t* _Nonnull pPlanes[2], int x, int y, int width, int height, int priority, SpriteID* _Nonnull pOutSpriteId);
extern errno_t Screen_RelinquishSprite(Screen* _Nonnull self, SpriteID spriteId);
extern errno_t Screen_SetSpritePosition(Screen* _Nonnull self, SpriteID spriteId, int x, int y);
extern errno_t Screen_SetSpriteVisible(Screen* _Nonnull self, SpriteID spriteId, bool isVisible);

#define Screen_SetNeedsUpdate(__self) \
((__self)->flags.isNewCopperProgNeeded = 1)

#endif /* Screen_h */

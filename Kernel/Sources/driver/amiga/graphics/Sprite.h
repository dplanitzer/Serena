//
//  Sprite.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Sprite_h
#define Sprite_h

#include <kern/errno.h>
#include <kern/types.h>
#include "VideoConfiguration.h"


#define NUM_HARDWARE_SPRITES    8

#define MIN_SPRITE_WIDTH        16
#define MAX_SPRITE_WIDTH        16

#define MIN_SPRITE_HEIGHT       0
#define MAX_SPRITE_HEIGHT       511

#define MIN_SPIRTE_VPOS         0
#define MAX_SPRITE_VPOS         511

#define MIN_SPRITE_HPOS         0
#define MAX_SPRITE_HPOS         511


typedef struct Sprite {
    uint16_t* _Nonnull  data;   // sprxctl, sprxctl, (plane0, plane1)..., 0, 0
    int16_t             x;
    int16_t             y;
    uint16_t            height;
    bool                isVisible;
} Sprite;


extern errno_t Sprite_Create(int width, int height, PixelFormat pixelFormat, Sprite* _Nonnull * _Nonnull pOutSelf);
extern void Sprite_Destroy(Sprite* _Nullable self);

extern void Sprite_SetPixels(Sprite* _Nonnull self, const uint16_t* _Nullable planes[2]);

extern void Sprite_SetPosition(Sprite* _Nonnull self, int16_t x, int16_t y);
extern void Sprite_SetVisible(Sprite* _Nonnull self, bool isVisible);

#endif /* Sprite_h */

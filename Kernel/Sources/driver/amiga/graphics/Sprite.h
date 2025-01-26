//
//  Sprite.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Sprite_h
#define Sprite_h

#include <klib/Error.h>
#include <klib/Types.h>
#include "ScreenConfiguration.h"


typedef int SpriteID;

#define NUM_HARDWARE_SPRITES    8
#define MAX_SPRITE_WIDTH        16
#define MAX_SPRITE_HEIGHT       511

typedef struct Sprite {
    uint16_t*     data;   // sprxctl, sprxctl, (plane0, plane1)..., 0, 0
    int16_t       x;
    int16_t       y;
    uint16_t      height;
    bool        isVisible;
} Sprite;


extern errno_t Sprite_Create(const uint16_t* _Nonnull pPlanes[2], int height, Sprite* _Nonnull * _Nonnull pOutSprite);
extern void Sprite_Destroy(Sprite* _Nullable pSprite);

extern void Sprite_SetPosition(Sprite* _Nonnull pSprite, int x, int y, const ScreenConfiguration* pConfig);
extern void Sprite_SetVisible(Sprite* _Nonnull pSprite, bool isVisible, const ScreenConfiguration* pConfig);

#endif /* Sprite_h */

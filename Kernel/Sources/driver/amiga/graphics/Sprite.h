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


#define NUM_HARDWARE_SPRITES    8
#define MAX_SPRITE_WIDTH        16
#define MAX_SPRITE_HEIGHT       511

typedef struct Sprite {
    uint16_t* _Nonnull  data;   // sprxctl, sprxctl, (plane0, plane1)..., 0, 0
    int16_t             x;
    int16_t             y;
    uint16_t            height;
    uint8_t             hDiwStart;
    uint8_t             vDiwStart;
    uint8_t             hShift;
    uint8_t             vShift;
    bool                isVisible;
} Sprite;


extern errno_t Sprite_Create(const uint16_t* _Nonnull pPlanes[2], int height, const ScreenConfiguration* _Nonnull cfg, Sprite* _Nonnull * _Nonnull pOutSelf);
extern void Sprite_Destroy(Sprite* _Nullable self);

extern void Sprite_SetPosition(Sprite* _Nonnull self, int x, int y);
extern void Sprite_SetVisible(Sprite* _Nonnull self, bool isVisible);

#endif /* Sprite_h */

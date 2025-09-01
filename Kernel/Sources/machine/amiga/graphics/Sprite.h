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
#include <kpi/fb.h>
#include <machine/amiga/chipset.h>


typedef struct Sprite {
    uint16_t* _Nonnull  data;   // sprxctl, sprxctl, (plane0, plane1)..., 0, 0
    int16_t             x;
    int16_t             y;
    uint16_t            height;
    bool                isVisible;
    bool                isAcquired;
} Sprite;


extern void Sprite_Init(Sprite* _Nonnull self);

extern errno_t Sprite_Acquire(Sprite* _Nonnull self, int width, int height, PixelFormat pixelFormat);
extern void Sprite_Relinquish(Sprite* _Nonnull self);

#define Sprite_IsNull(__self) \
((__self)->height == 0)

extern void Sprite_SetPixels(Sprite* _Nonnull self, const uint16_t* _Nullable planes[2]);

extern void Sprite_SetPosition(Sprite* _Nonnull self, int16_t x, int16_t y);
extern void Sprite_SetVisible(Sprite* _Nonnull self, bool isVisible);

#endif /* Sprite_h */

//
//  Screen.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Screen_h
#define Screen_h

#include <kern/errno.h>
#include <kern/types.h>
#include <klib/List.h>
#include "CopperProgram.h"
#include "VideoConfiguration.h"
#include "Sprite.h"
#include "Surface.h"


typedef struct CLUTEntry {
    uint8_t     r;
    uint8_t     g;
    uint8_t     b;
    uint8_t     flags;
} CLUTEntry;
#define MAX_CLUT_ENTRIES    32


enum {
    kScreenFlag_IsNewCopperProgNeeded = 0x01,   // We need to re-compile the current screen state into a new Copper program
    kScreenFlag_IsVisible = 0x02,               // Is visible on screen
};


typedef struct Screen {
    ListNode                chain;
    Sprite* _Nonnull        nullSprite;
    Sprite* _Nonnull        sprite[NUM_HARDWARE_SPRITES];
    Surface* _Nullable      surface;        // The screen pixels
    CLUTEntry* _Nullable    clut;           // The screen color lookup table
    int16_t                 clutEntryCount;
    uint16_t                flags;
    VideoConfiguration      vidConfig;
    int                     id;
    int16_t                 hDiwStart;      // Visible screen space origin and sprite scaling
    int16_t                 vDiwStart;
    int16_t                 hSprScale;
    int16_t                 vSprScale;
} Screen;


extern errno_t Screen_Create(int id, const VideoConfiguration* _Nonnull vidCfg, Surface* _Nonnull srf, Sprite* _Nonnull pNullSprite, Screen* _Nullable * _Nonnull pOutSelf);
extern void Screen_Destroy(Screen* _Nullable pScreen);

#define Screen_GetId(__self) \
((__self)->id)

#define Screen_SetNeedsUpdate(__self) \
((__self)->flags |= kScreenFlag_IsNewCopperProgNeeded)

#define Screen_NeedsUpdate(__self) \
(((__self)->flags & kScreenFlag_IsNewCopperProgNeeded) == kScreenFlag_IsNewCopperProgNeeded)

#define Screen_GetVideoConfiguration(__self) \
(&(__self)->vidConfig)

#define Screen_IsInterlaced(__self) \
VideoConfiguration_IsInterlaced(&(__self)->vidConfig)

#define Screen_SetVisible(__self, __flag) \
if (__flag) { \
    (__self)->flags |= kScreenFlag_IsVisible; \
} else { \
    (__self)->flags &= ~kScreenFlag_IsVisible; \
}

// Returns true if this screen is currently visible
#define Screen_IsVisible(__self) \
((((__self)->flags & kScreenFlag_IsVisible) == kScreenFlag_IsVisible) ? true : false)

extern void Screen_GetPixelSize(Screen* _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight);

extern errno_t Screen_SetCLUTEntry(Screen* _Nonnull self, size_t idx, RGBColor32 color);
extern errno_t Screen_SetCLUTEntries(Screen* _Nonnull self, size_t idx, size_t count, const RGBColor32* _Nonnull entries);

extern errno_t Screen_AcquireSprite(Screen* _Nonnull self, int width, int height, PixelFormat pixelFormat, int priority, int* _Nonnull pOutSpriteIdx);
extern errno_t Screen_RelinquishSprite(Screen* _Nonnull self, int sprIdx);
extern errno_t Screen_SetSpritePixels(Screen* _Nonnull self, int sprIdx, const uint16_t* _Nonnull planes[2]);
extern errno_t Screen_SetSpritePosition(Screen* _Nonnull self, int sprIdx, int x, int y);
extern errno_t Screen_SetSpriteVisible(Screen* _Nonnull self, int sprIdx, bool isVisible);

extern size_t Screen_CalcCopperProgramLength(Screen* _Nonnull self);
extern CopperInstruction* _Nonnull Screen_MakeCopperProgram(Screen* _Nonnull self, CopperInstruction* _Nonnull ip, Sprite* _Nullable mouseCursor, bool isLightPenEnabled, bool isOddField);

#endif /* Screen_h */

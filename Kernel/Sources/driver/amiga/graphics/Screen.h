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
#include "CopperProgram.h"
#include "PixelFormat.h"
#include "ScreenConfiguration.h"
#include "Sprite.h"
#include "Surface.h"


// Specifies what you want to do with the pixels when you call LockPixels()
typedef enum PixelAccess {
    kPixelAccess_Read,
    kPixelAccess_ReadWrite
} PixelAccess;


typedef struct Screen {
    Surface* _Nullable                  surface;        // the screen framebuffer
    const ScreenConfiguration* _Nonnull screenConfig;
    PixelFormat                         pixelFormat;
    Sprite* _Nonnull                    nullSprite;
    Sprite* _Nonnull                    sprite[NUM_HARDWARE_SPRITES];
    struct __ScreenFlags {
        unsigned int        isNewCopperProgNeeded:1;
        unsigned int        isSurfaceLocked:1;
        unsigned int        reserved:30;
    }                                   flags;
} Screen;


extern errno_t Screen_Create(const ScreenConfiguration* _Nonnull pConfig, PixelFormat pixelFormat, Sprite* _Nonnull pNullSprite, Screen* _Nullable * _Nonnull pOutSelf);
extern void Screen_Destroy(Screen* _Nullable pScreen);

#define Screen_SetNeedsUpdate(__self) \
((__self)->flags.isNewCopperProgNeeded = 1)

#define Screen_GetConfiguration(__self) \
((__self)->screenConfig)

#define Screen_IsInterlaced(__self) \
ScreenConfiguration_IsInterlaced((__self)->screenConfig)

extern void Screen_GetPixelSize(Screen* _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight);

extern errno_t Screen_SetCLUTEntry(Screen* _Nonnull self, size_t idx, RGBColor32 color);
extern errno_t Screen_SetCLUTEntries(Screen* _Nonnull self, size_t idx, size_t count, const RGBColor32* _Nonnull entries);

extern errno_t Screen_LockPixels(Screen* _Nonnull self, PixelAccess access, void* _Nonnull plane[8], size_t bytesPerRow[8], size_t* _Nonnull pOutPlaneCount);
extern errno_t Screen_UnlockPixels(Screen* _Nonnull self);

extern errno_t Screen_AcquireSprite(Screen* _Nonnull self, const uint16_t* _Nonnull pPlanes[2], int x, int y, int width, int height, int priority, int* _Nonnull pOutSpriteId);
extern errno_t Screen_RelinquishSprite(Screen* _Nonnull self, int spriteId);
extern errno_t Screen_SetSpritePosition(Screen* _Nonnull self, int spriteId, int x, int y);
extern errno_t Screen_SetSpriteVisible(Screen* _Nonnull self, int spriteId, bool isVisible);

extern size_t Screen_CalcCopperProgramLength(Screen* _Nonnull self);
extern CopperInstruction* _Nonnull Screen_MakeCopperProgram(Screen* _Nonnull self, CopperInstruction* _Nonnull pCode, bool isLightPenEnabled, bool isOddField);

#endif /* Screen_h */

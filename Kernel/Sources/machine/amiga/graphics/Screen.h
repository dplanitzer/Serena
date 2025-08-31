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
#include "ColorTable.h"
#include "VideoConfiguration.h"
#include "Surface.h"


enum {
    kScreenFlag_IsNewCopperProgNeeded = 0x01,   // We need to re-compile the current screen state into a new Copper program
    kScreenFlag_IsVisible = 0x02,               // Is visible on screen
};


typedef struct Screen {
    ListNode                chain;
    Surface* _Nullable      surface;        // The screen pixels
    ColorTable* _Nullable   clut;           // The screen color lookup table
    uint16_t                flags;
    VideoConfiguration      vidConfig;
    int                     id;
} Screen;


extern errno_t Screen_Create(int id, const VideoConfiguration* _Nonnull vidCfg, Surface* _Nonnull srf, Screen* _Nullable * _Nonnull pOutSelf);
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

#endif /* Screen_h */

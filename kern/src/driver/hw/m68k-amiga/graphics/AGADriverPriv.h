//
//  AGADriverPriv.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef AGADriverPriv_h
#define AGADriverPriv_h

#include "AGADriver.h"
#include <sched/mtx.h>
#include "gd.h"


final_class_ivars(AGADriver, IODriver,
);

extern errno_t _AGADriver_CreateBuffer(AGADriverRef _Nonnull _Locked self, int width, int height, vio_pixfmt_t pixelFormat, Surface* _Nullable * _Nonnull pOutSurface);
extern errno_t _AGADriver_CreateCLUT(AGADriverRef _Nonnull _Locked self, size_t colorDepth, vio_rgb32_t defaultColor, ColorTable* _Nullable * _Nonnull pOutClut);


extern void AGADriver_CopperManager(AGADriverRef _Nonnull self);


extern errno_t _AGADriver_BindSprite(AGADriverRef _Nonnull _Locked self, int unit, Surface* _Nullable srf);

// Screens
extern void AGADriver_getScreenSize(AGADriverRef _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight);
extern void AGADriver_setScreenConfigObserver(AGADriverRef _Nonnull self, vcpu_t _Nullable vp, int signo);


// Light Pen
extern void AGADriver_setLightPenEnabled(AGADriverRef _Nonnull self, bool enabled);


// Mouse Cursor

extern errno_t AGADriver_obtainCursor(AGADriverRef _Nonnull self);
extern void AGADriver_releaseCursor(AGADriverRef _Nonnull self);
extern errno_t AGADriver_bindCursor(AGADriverRef _Nonnull self, int id);
extern void AGADriver_setCursorPosition(AGADriverRef _Nonnull self, int x, int y);
extern void AGADriver_setCursorVisible(AGADriverRef _Nonnull self, bool isVisible);

#endif /* AGADriverPriv_h */

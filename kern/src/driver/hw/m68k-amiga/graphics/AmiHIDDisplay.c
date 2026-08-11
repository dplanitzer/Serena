//
//  AmiHIDDisplay.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "AmiHIDDisplay.h"
#include "gd.h"
#include <kpi/hid.h>
#include <kpi/process.h>

IOCATS_DEF(g_cats, IOHID_DISPLAY);


errno_t AmiHIDDisplay_Create(AmiHIDDisplayRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    AmiHIDDisplayRef self;
    
    try(IODriver_Create(class(AmiHIDDisplay), g_cats, (IODriverRef*)&self));

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

errno_t AmiHIDDisplay_setCursor(AmiHIDDisplayRef _Nonnull self, const IOHIDCursor* _Nullable cursor)
{
    gdLock();
    const errno_t err = gdSetCursor(cursor);
    gdUnlock();
    return err;
}

void AmiHIDDisplay_updateCursor(AmiHIDDisplayRef _Nonnull self, int16_t x, int16_t y, unsigned int flags)
{
    gdLock();
    gdUpdateCursor(x, y, flags);
    gdUnlock();
}

void AmiHIDDisplay_getScreenResolution(AmiHIDDisplayRef _Nonnull self, int16_t* _Nonnull pOutWidth, int16_t* _Nonnull pOutHeight)
{
    gd_display_mode_t mode;

    gdLock();
    gdGetDisplayInfo(GD_DISPLAY_MODE, &mode);
    gdUnlock();
    
    *pOutWidth = mode.width;
    *pOutHeight = mode.height;
}

void AmiHIDDisplay_setChangeSignal(AmiHIDDisplayRef _Nonnull self, vcpu_t _Nullable vp, int signo)
{
    gdLock();
    gdSetDisplayChangeSignal(vp, signo);
    gdUnlock();
}


class_func_defs(AmiHIDDisplay, IOHIDDisplay,
override_func_def(getScreenResolution, AmiHIDDisplay, IOHIDDisplay)
override_func_def(setChangeSignal, AmiHIDDisplay, IOHIDDisplay)
override_func_def(setCursor, AmiHIDDisplay, IOHIDDisplay)
override_func_def(updateCursor, AmiHIDDisplay, IOHIDDisplay)
);

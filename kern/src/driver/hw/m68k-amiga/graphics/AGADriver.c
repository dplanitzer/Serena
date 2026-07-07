//
//  AGADriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "AGADriverPriv.h"
#include "copper.h"
#include <string.h>
#include <driver/IOLib.h>
#include <hal/irq.h>
#include <handler/IOGraphicsHandler.h>

IOCATS_DEF(g_cats, IOVID_FB);


// Creates a graphics driver instance which manages the on-board video hardware.
// We assume that video is turned off at the time this function is called and
// video remains turned off until a screen has been created and is made the
// current screen.
errno_t AGADriver_Create(AGADriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    AGADriverRef self;
    
    try(IODriver_Create(class(AGADriver), g_cats, (IODriverRef*)&self));
    mtx_init(&self->io_mtx);

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

errno_t AGADriver_start(AGADriverRef _Nonnull self)
{
    decl_try_err();

    // Create a null Copper program and null sprite
    copper_prog_t nullCopperProg;
    try(Surface_CreateNullSprite(&self->nullSpriteSurface));
    try(AGADriver_CreateNullCopperProg(self, &nullCopperProg));
    for (int i = 0; i < SPRITE_COUNT; i++) {
        self->spriteChannel[i].isVisible = true;
    }


    // Allocate the Copper management VCPU
    wq_init(&self->copvpWaitQueue);
    self->copvpSigs = sig_bit(SIGCOPRUN);

    try(IOAcquireVirtualProcessor((vcpu_func_t)AGADriver_CopperManager, self, VCPU_QOS_URGENT, VCPU_PRI_NORMAL, &self->copvp));

    
    // Initialize the Copper scheduler
    copper_init(nullCopperProg, SIGCOPRUN, self->copvp);
    copper_start();

    IOResumeVirtualProcessor(self->copvp);
    
catch:
    return err;
}

bool AGADriver_isExclusive(AGADriverRef _Nonnull self)
{
    //XXX tmp as long as the console is still inside the kernel
    return false;
}

errno_t AGADriver_getDFSInfo(AGADriverRef _Nonnull self, IODFSInfo* _Nonnull info)
{
    strcpy(info->name, "fb");
    info->func = IOGraphicsHandler_Create;
    info->uid = UID_ROOT;
    info->gid = GID_ROOT;
    info->perms = fs_perms_from_octal(0666);

    return EOK;
}


class_func_defs(AGADriver, DisplayDriver,
override_func_def(start, AGADriver, IODriver)
override_func_def(isExclusive, AGADriver, IODriver)
override_func_def(getDFSInfo, AGADriver, IODriver)
override_func_def(getScreenSize, AGADriver, DisplayDriver)
override_func_def(setScreenConfigObserver, AGADriver, DisplayDriver)
override_func_def(setLightPenEnabled, AGADriver, DisplayDriver)
override_func_def(obtainCursor, AGADriver, DisplayDriver)
override_func_def(releaseCursor, AGADriver, DisplayDriver)
override_func_def(bindCursor, AGADriver, DisplayDriver)
override_func_def(setCursorPosition, AGADriver, DisplayDriver)
override_func_def(setCursorVisible, AGADriver, DisplayDriver)
);

//
//  GraphicsDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "GraphicsDriverPriv.h"
#include "copper.h"


errno_t _GraphicsDriver_CreateCLUT(GraphicsDriverRef _Nonnull _Locked self, size_t colorDepth, RGBColor32 defaultColor, ColorTable* _Nullable * _Nonnull pOutClut)
{
    ColorTable* clut;

    const errno_t err = ColorTable_Create(_GraphicsDriver_GetNewGObjId(self), colorDepth, defaultColor, &clut);
    if (err == EOK) {
        List_InsertBeforeFirst(&self->gobjs, GObject_GetChainPtr(clut));
        *pOutClut = clut;
    }

    return err;
}

errno_t GraphicsDriver_CreateCLUT(GraphicsDriverRef _Nonnull self, size_t colorDepth, int* _Nonnull pOutId)
{
    ColorTable* clut;

    mtx_lock(&self->io_mtx);
    const errno_t err = _GraphicsDriver_CreateCLUT(self, colorDepth, kRGBColor32_Black, &clut);
    if (err == EOK) {
        *pOutId = GObject_GetId(clut);
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_DestroyCLUT(GraphicsDriverRef _Nonnull self, int id)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    ColorTable* clut = _GraphicsDriver_GetClutForId(self, id);

    if (clut) {
        if (g_copper_running_prog->res.clut != clut) {
            _GraphicsDriver_DestroyGObj(self, clut);
        }
        else {
            err = EBUSY;
        }
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_GetCLUTInfo(GraphicsDriverRef _Nonnull self, int id, CLUTInfo* _Nonnull pOutInfo)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    ColorTable* clut = _GraphicsDriver_GetClutForId(self, id);

    if (clut) {
        pOutInfo->entryCount = clut->entryCount;
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return EOK;
}

// Sets the contents of 'count' consecutive CLUT entries starting at index 'idx'
// to the colors in the array 'entries'.
errno_t GraphicsDriver_SetCLUTEntries(GraphicsDriverRef _Nonnull self, int id, size_t idx, size_t count, const RGBColor32* _Nonnull entries)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    ColorTable* clut = _GraphicsDriver_GetClutForId(self, id);
    if (clut) {
        err = ColorTable_SetEntries(clut, idx, count, entries);
        if (err == EOK && clut == (ColorTable*)g_copper_running_prog->res.clut) {
            copper_prog_t prog = _GraphicsDriver_GetEditableCopperProg(self);

            if (prog) {
                copper_prog_clut_changed(prog, idx, count);
                copper_schedule(prog, 0);
            }
        }
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

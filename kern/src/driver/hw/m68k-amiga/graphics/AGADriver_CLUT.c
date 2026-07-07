//
//  AGADriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "AGADriverPriv.h"
#include "copper.h"


errno_t AGADriver_CreateCLUT(AGADriverRef _Nonnull self, size_t colorDepth, int* _Nonnull pOutId)
{
    ColorTable* clut;

    mtx_lock(&self->io_mtx);
    const errno_t err = ColorTable_Create(colorDepth, kRGBColor32_Black, &clut);
    if (err == EOK) {
        *pOutId = ColorTable_GetId(clut);
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t AGADriver_DestroyCLUT(AGADriverRef _Nonnull self, int id)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    ColorTable* clut = ColorTable_GetForId(id);

    if (clut == NULL) {
        throw(EINVAL);
    }
    if (g_copper_running_prog->res.clut == clut) {
        throw(EBUSY);
    }

    ColorTable_DelRef(clut);

catch:
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t AGADriver_GetCLUTInfo(AGADriverRef _Nonnull self, int id, clut_info_t* _Nonnull pOutInfo)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    ColorTable* clut = ColorTable_GetForId(id);

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
errno_t AGADriver_SetCLUTEntries(AGADriverRef _Nonnull self, int id, size_t idx, size_t count, const color_rgb32_t* _Nonnull entries)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    ColorTable* clut = ColorTable_GetForId(id);
    
    if (clut) {
        err = ColorTable_SetEntries(clut, idx, count, entries);
        if (err == EOK && clut == (ColorTable*)g_copper_running_prog->res.clut) {
            copper_prog_t prog = _AGADriver_GetEditableCopperProg(self);

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

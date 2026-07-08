//
//  gd_clut.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd.h"


errno_t gdGenClut(size_t colorDepth, int* _Nonnull pOutId)
{
    ColorTable* clut;

    const errno_t err = ColorTable_Create(colorDepth, kRGBColor32_Black, &clut);
    if (err == EOK) {
        *pOutId = ColorTable_GetId(clut);
    }
    return err;
}

errno_t gdDeleteClut(int id)
{
    ColorTable* clut = ColorTable_GetForId(id);

    if (clut == NULL) {
        return EINVAL;
    }
    if (g_copper_running_prog->res.clut == clut) {
        return EBUSY;
    }

    ColorTable_DelRef(clut);

    return EOK;
}

errno_t gdGetClutInfo(int id, clut_info_t* _Nonnull pOutInfo)
{
    ColorTable* clut = ColorTable_GetForId(id);

    if (clut) {
        pOutInfo->entryCount = clut->entryCount;
        return EOK;
    }
    else {
        return EINVAL;
    }
}

// Sets the contents of 'count' consecutive CLUT entries starting at index 'idx'
// to the colors in the array 'entries'.
errno_t gdSetClutEntries(int id, size_t idx, size_t count, const color_rgb32_t* _Nonnull entries)
{
    decl_try_err();
    ColorTable* clut = ColorTable_GetForId(id);
    
    if (clut == NULL) {
        return EINVAL;
    }

    err = ColorTable_SetEntries(clut, idx, count, entries);
    if (err == EOK && clut == (ColorTable*)g_copper_running_prog->res.clut) {
        copper_prog_t prog = copper_get_editable_prog();

        if (prog) {
            copper_prog_clut_changed(prog, idx, count);
            copper_schedule(prog, 0);
        }
    }

    return err;
}

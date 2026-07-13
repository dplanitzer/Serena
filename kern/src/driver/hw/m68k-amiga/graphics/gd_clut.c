//
//  gd_clut.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd_priv.h"
#include <kern/kalloc.h>


static int      g_next_clut_id = 1;
static deque_t  g_clut_table;


clut_t* _Nullable _clut_for_id(int id)
{
    deque_for_each(&g_clut_table, struct clut, it,
        if (it->id == id) {
            return it;
        }
    )
    return NULL;
}


errno_t gdGenClut(size_t entryCount, int* _Nonnull pOutId)
{
    decl_try_err();
    clut_t* self;
    
    switch (entryCount) {
        case 2:
        case 4:
        case 8:
        case 16:
        case 32:
//        case 64:
//        case 128:
//        case 256:
            break;

        default:
            return EINVAL;
    }

    try(kalloc_cleared(sizeof(struct clut) + (entryCount - 1) * sizeof(uint16_t), (void**) &self));
    self->id = g_next_clut_id++;
    self->entryCount = entryCount;
    
    deque_add_first(&g_clut_table, &self->chain);
    *pOutId = self->id;

catch:
    return err;
}

void _gdDestroyClut(clut_t* _Nullable clut)
{
    if (clut) {
        deque_remove(&g_clut_table, &clut->chain);
        kfree(clut);
    }
}

errno_t gdDeleteClut(int id)
{
    clut_t* clut = _clut_for_id(id);

    if (clut == NULL) {
        return EINVAL;
    }
    if (g_copper_running_prog->res.clut == clut) {
        return EBUSY;
    }

    _gdDestroyClut(clut);
    return EOK;
}

errno_t gdGetClutInfo(int id, vio_clut_info_t* _Nonnull pOutInfo)
{
    clut_t* clut = _clut_for_id(id);

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
errno_t gdSetClutEntries(int id, size_t idx, size_t count, const vio_rgb32_t* _Nonnull entries)
{
    decl_try_err();
    clut_t* clut = _clut_for_id(id);
    
    if (clut == NULL) {
        return EINVAL;
    }

    if (idx + count > clut->entryCount) {
        return EINVAL;
    }


    if (count > 0) {
        for (size_t i = 0; i < count; i++) {
            const vio_rgb32_t color = entries[i];
            const uint16_t r = VIO_RGB32_RED(color);
            const uint16_t g = VIO_RGB32_GREEN(color);
            const uint16_t b = VIO_RGB32_BLUE(color);

            clut->entry[idx + i] = (r >> 4 & 0x0f) << 8 | (g >> 4 & 0x0f) << 4 | (b >> 4 & 0x0f);
        }

        if (clut == g_copper_running_prog->res.clut) {
            copper_prog_t prog = copper_get_editable_prog();

            if (prog) {
                copper_prog_clut_changed(prog, idx, count);
                copper_schedule(prog, 0);
            }
        }
    }

    return EOK;
}

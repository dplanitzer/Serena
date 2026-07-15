//
//  gd_framebuffer.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd_priv.h"
#include <kern/kalloc.h>


static int      g_next_fb_id = 1;
static deque_t  g_fb_list;


framebuffer_t* _Nullable _fb_for_id(int id)
{
    deque_for_each(&g_fb_list, struct framebuffer, it,
        if (it->id == id) {
            return it;
        }
    )
    return NULL;
}


errno_t gdGenFramebuffer(size_t entryCount, int* _Nonnull pOutId)
{
    decl_try_err();
    framebuffer_t* fb;
    
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

    try(kalloc_cleared(sizeof(struct framebuffer) + (entryCount - 1) * sizeof(uint16_t), (void**) &fb));
    fb->id = g_next_fb_id++;
    fb->clut_size = entryCount;
    
    deque_add_first(&g_fb_list, &fb->chain);
    *pOutId = fb->id;

catch:
    return err;
}

void _gdDestroyFramebuffer(framebuffer_t* _Nullable fb)
{
    if (fb) {
        deque_remove(&g_fb_list, &fb->chain);
        kfree(fb);
    }
}

errno_t gdDeleteFramebuffer(int id)
{
    framebuffer_t* fb = _fb_for_id(id);

    if (fb == NULL) {
        return EINVAL;
    }
    if (g_copper_running_prog->res.fb == fb) {
        return EBUSY;
    }

    _gdDestroyFramebuffer(fb);
    return EOK;
}

errno_t gdGetFramebufferInfo(int id, vio_clut_info_t* _Nonnull pOutInfo)
{
    framebuffer_t* fb = _fb_for_id(id);

    if (fb) {
        pOutInfo->entryCount = fb->clut_size;
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
    framebuffer_t* fb = _fb_for_id(id);
    
    if (fb == NULL) {
        return EINVAL;
    }

    if (idx + count > fb->clut_size) {
        return EINVAL;
    }


    if (count > 0) {
        for (size_t i = 0; i < count; i++) {
            const vio_rgb32_t color = entries[i];
            const uint16_t r = VIO_RGB32_RED(color);
            const uint16_t g = VIO_RGB32_GREEN(color);
            const uint16_t b = VIO_RGB32_BLUE(color);

            fb->clut[idx + i] = (r >> 4 & 0x0f) << 8 | (g >> 4 & 0x0f) << 4 | (b >> 4 & 0x0f);
        }

        if (fb == g_copper_running_prog->res.fb) {
            copper_prog_t prog = copper_get_editable_prog();

            if (prog) {
                copper_prog_clut_changed(prog, idx, count);
                copper_schedule(prog, 0);
            }
        }
    }

    return EOK;
}

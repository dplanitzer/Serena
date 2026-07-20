//
//  gd_framebuffer.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd_priv.h"
#include <kern/kalloc.h>

static int                  g_next_fb_id = 1;
static deque_t              g_fb_list;
static vcpu_t _Nullable     g_screen_conf_observer;
static int                  g_screen_conf_signal;
framebuffer_t* _Nullable    g_cur_fb;
uint16_t                    g_clut[CLUT_SIZE];
uint8_t                     g_clut_size = CLUT_SIZE;


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
    
    try(kalloc_cleared(sizeof(struct framebuffer), (void**) &fb));
    fb->id = g_next_fb_id++;
    
    deque_add_first(&g_fb_list, &fb->chain);
    *pOutId = fb->id;

catch:
    return err;
}

void _gdDestroyFramebuffer(framebuffer_t* _Nullable fb)
{
    if (fb) {
        Surface_DelRef(fb->front_buf);
        fb->front_buf = NULL;

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

errno_t gdAttachBuffer(int fb_id, int buf_id)
{
    framebuffer_t* fb = _fb_for_id(fb_id);
    Surface* pbo = Surface_GetForId(buf_id);

    if (fb == NULL || (pbo == NULL && buf_id > 0)) {
        return EINVAL;
    }
    if (g_copper_running_prog->res.fb == fb) {
        return EBUSY;
    }

    // Note that if there's a Copper program scheduled right now, that this one
    // can not reference 'buf_id' as a front buffer because framebuffer switches
    // are done atomically in gdSetCurrentFramebuffer().


    // Detach the old pixel buffer
    if (fb->front_buf) {
        Surface_DelRef(fb->front_buf);
        fb->front_buf = NULL;
    }


    // Attach the new buffer, if requested
    if (pbo) {
        fb->front_buf = pbo;
        Surface_AddRef(pbo);
    }

    return EOK;
}

errno_t gdGetFramebufferInfo(int id, vio_clut_info_t* _Nonnull pOutInfo)
{
    framebuffer_t* fb = _fb_for_id(id);

    if (fb) {
        pOutInfo->entryCount = g_clut_size;
        return EOK;
    }
    else {
        return EINVAL;
    }
}

errno_t gdClut(size_t idx, size_t count, const vio_rgb32_t* _Nonnull entries)
{
    if (idx + count > g_clut_size) {
        return EINVAL;
    }


    if (count > 0) {
        for (size_t i = 0; i < count; i++) {
            const vio_rgb32_t color = entries[i];
            const uint16_t r = VIO_RGB32_RED(color);
            const uint16_t g = VIO_RGB32_GREEN(color);
            const uint16_t b = VIO_RGB32_BLUE(color);

            g_clut[idx + i] = (r >> 4 & 0x0f) << 8 | (g >> 4 & 0x0f) << 4 | (b >> 4 & 0x0f);
        }

        copper_prog_t prog = copper_get_editable_prog();

        if (prog) {
            copper_prog_clut_changed(prog, idx, count);
            copper_schedule(prog, 0);
        }
    }

    return EOK;
}

errno_t gdGetClut(size_t idx, size_t count, vio_rgb32_t* _Nonnull entries)
{
    if (idx + count > g_clut_size) {
        return EINVAL;
    }


    for (size_t i = 0; i < count; i++) {
        const uint16_t color = g_clut[idx + i];
        const uint16_t r = ((color >> 8) & 0x0f) << 4;
        const uint16_t g = ((color >> 4) & 0x0f) << 4;
        const uint16_t b = (color & 0x0f) << 4;

        entries[i] = VIO_RGB32_MAKE(r, g, b);
    }

    return EOK;
}

errno_t gdGetClutInfo(gd_clut_info_t* _Nonnull info)
{
    info->entryCount = g_clut_size;
    info->redBits = 4;
    info->blueBits = 4;
    info->greenBits = 4;
    return EOK;
}

static errno_t _validate_fb(framebuffer_t* _Nonnull fb, const video_conf_t* _Nullable * _Nonnull pOutVc)
{
    decl_try_err();
    const video_conf_t* vc = NULL;
    Surface* front_buf = fb->front_buf;

    if (front_buf == NULL) {
        return EINVAL;  //XXX ENOBUF?
    }

    if ((vc = get_matching_video_conf(
        Surface_GetWidth(front_buf),
        Surface_GetHeight(front_buf),
        Surface_GetPixelFormat(front_buf))) == NULL) {
            return ENOTSUP; //XXX ENOMATCH?
    }

    *pOutVc = vc;
    return EOK;
}


errno_t gdSetCurrentFramebuffer(int fb_id)
{
    decl_try_err();
    framebuffer_t* fb = _fb_for_id(fb_id);
    const video_conf_t* vc = NULL;
    copper_prog_t prog = NULL;
    
    if (fb == NULL && fb_id > 0) {
        return EINVAL;
    }
    if (fb && g_copper_running_prog->res.fb == fb) {
        return EBUSY;
    }


    // Compile the Copper program(s) for the new framebuffer
    if (fb) {
        try(_validate_fb(fb, &vc));
        try(create_screen_copper_prog(vc, fb->front_buf, fb, &prog));
    }
    else {
        try(create_null_copper_prog(&prog));
    } 


    // Schedule the new Copper program and wait until the new program is running
    // and the previous one has been retired. It's save to deallocate the old
    // framebuffer once the old program has stopped running.
    copper_schedule(prog, COPFLAG_WAIT_RUNNING);
    g_cur_fb = fb;


    if (g_screen_conf_observer) {
        vcpu_send_signal(g_screen_conf_observer, g_screen_conf_signal);
    }


catch:
    return err;
}

int gdGetCurrentFramebuffer(void)
{
    return (g_cur_fb) ? g_cur_fb->id : 0;
}

int gdGetScreenbuffer(void)
{
    return (g_cur_fb) ? g_cur_fb->front_buf->id : 0;
}

void gdGetScreenSize(int* _Nonnull pOutWidth, int* _Nonnull pOutHeight)
{
    const video_conf_t* vc = g_copper_running_prog->video_conf;

    *pOutWidth = vc->width;
    *pOutHeight = vc->height;
}

void gdSetScreenConfigObserver(vcpu_t _Nullable vp, int signo)
{
    g_screen_conf_observer = vp;
    g_screen_conf_signal = signo;
}

void gdSetLightPenEnabled(bool enabled)
{
    if (g_light_pen_enabled != enabled) {
        g_light_pen_enabled = enabled;

        copper_prog_t prog = copper_get_editable_prog();
        if (prog) {
            copper_prog_set_lp_enabled(prog, enabled);
            copper_schedule(prog, 0);
        }
    }
}

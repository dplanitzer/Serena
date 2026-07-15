//
//  gd_screen.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd_priv.h"
#include <hal/irq.h>

vcpu_t _Nullable g_screen_conf_observer;
int              g_screen_conf_signal;


static int _get_config_value(const intptr_t* _Nonnull config, int key, intptr_t def)
{
    while (*config != VIO_SCR_END) {
        if (*config == key) {
            return *(config + 1);
        }
        config += 2;
    }

    return def;
}

// Parses the given 'icfg' in order to get a CLUT that is suitable for the
// screen configuration.
static errno_t _get_fb_from_config(const intptr_t* _Nonnull icfg, framebuffer_t* _Nullable * _Nonnull pOutFb, bool* _Nonnull pOutCreated)
{
    decl_try_err();
    framebuffer_t* fb = NULL;
    int fb_id = _get_config_value(icfg, VIO_SCR_CLUT, -1);
    
    if (fb_id != -1) {
        fb = _fb_for_id(fb_id);
        if (fb == NULL) {
            return EINVAL;
        }

        if (fb->clut_size != COLOR_COUNT) {
            return ENOTSUP;
        }

        *pOutCreated = false;
    }
    else {
        err = gdGenFramebuffer(COLOR_COUNT, &fb_id);
        if (err != EOK) {
            return err;
        }

        *pOutFb = _fb_for_id(fb_id);
        *pOutCreated = true;
    }

    *pOutFb = fb;
    return EOK;
}

// Parses the given 'icfg' in order to get a surface that can be used as a
// framebuffer for the screen configuration.
static errno_t _get_framebuffer_from_config(const intptr_t* _Nonnull icfg, Surface* _Nullable * _Nonnull pOutSurface, const video_conf_t* _Nullable * _Nonnull pOutVc, bool* _Nonnull pOutCreated)
{
    decl_try_err();
    Surface* fb = NULL;
    const video_conf_t* vc = NULL;
    const fb_id = _get_config_value(icfg, VIO_SCR_FRAMEBUFFER, -1);

    if (fb_id != -1) {
        fb = Surface_GetForId(fb_id);
        if (fb == NULL) {
            return EINVAL;
        }

        if ((vc = get_matching_video_conf(
                Surface_GetWidth(fb),
                Surface_GetHeight(fb),
                Surface_GetPixelFormat(fb))) == NULL) {
            return ENOTSUP;
        }

        *pOutCreated = false;
    }
    else {
        const int w = _get_config_value(icfg, VIO_SCR_WIDTH, 0);
        const int h = _get_config_value(icfg, VIO_SCR_HEIGHT, 0);
        const int fmt = _get_config_value(icfg, VIO_SCR_PIXELFORMAT, 0);

        if (w <= 0 || h <= 0 || fmt == 0) {
            return EINVAL;
        }
        if ((vc = get_matching_video_conf(w, h, fmt)) == NULL) {
            return ENOTSUP;
        }

        err = Surface_Create(w, h, fmt, &fb);
        if (err != EOK) {
            return err;
        }

        *pOutCreated = true;
    }

    *pOutSurface = fb;
    *pOutVc = vc;
    return EOK;
}


// Sets the given screen as the current screen on the graphics driver. All graphics
// command apply to this new screen once this function has returned.
errno_t gdSetScreenConfig(const intptr_t* _Nullable icfg)
{
    decl_try_err();
    Surface* pbo = NULL;
    bool bPboCreated = false;
    framebuffer_t* fb = NULL;
    bool bFbCreated = false;
    const video_conf_t* vc = NULL;
    copper_prog_t prog = NULL;
    

    // Compile the Copper program(s) for the new screen
    if (icfg) {
        try(_get_fb_from_config(icfg, &fb, &bFbCreated));
        try(_get_framebuffer_from_config(icfg, &pbo, &vc, &bPboCreated));

        try(create_screen_copper_prog(vc, pbo, fb, &prog));
    }
    else {
        try(create_null_copper_prog(&prog));
    } 


    // Schedule the new Copper program and wait until the new program is running
    // and the previous one has been retired. It's save to deallocate the old
    // framebuffer once the old program has stopped running.
    copper_schedule(prog, COPFLAG_WAIT_RUNNING);


    if (g_screen_conf_observer) {
        vcpu_send_signal(g_screen_conf_observer, g_screen_conf_signal);
    }


catch:
    if (err != EOK) {
        if (bPboCreated) {
            Surface_DelRef(pbo);
        }
        if (bFbCreated) {
            _gdDestroyFramebuffer(fb);
        }
    }

    return err;
}

errno_t gdGetScreenConfig(intptr_t* _Nonnull conf, size_t bufsiz)
{
    size_t i = 0;

    if (bufsiz == 0) {
        return EINVAL;
    }

    // VIO_SCR_FRAMEBUFFER
    // VIO_SCR_CLUT (if the pixel format is one of the indirect formats)
    // VIO_SCR_WIDTH
    // VIO_SCR_HEIGHT
    // VIO_SCR_PIXELFORMAT
    // VIO_SCR_END
    if (bufsiz < 11) {
        return ERANGE;
    }

    const unsigned sim = irq_set_mask(IRQ_MASK_VBLANK);
    const video_conf_t* vc = g_copper_running_prog->video_conf;
    Surface* pbo = g_copper_running_prog->res.pbo;
    framebuffer_t* fb = g_copper_running_prog->res.fb;
    irq_restore_mask(sim);

    conf[i++] = VIO_SCR_FRAMEBUFFER;
    conf[i++] = (pbo) ? Surface_GetId(pbo) : 0;
    conf[i++] = VIO_SCR_CLUT;
    conf[i++] = (fb) ? fb->id : 0;
    conf[i++] = VIO_SCR_WIDTH;
    conf[i++] = vc->width;
    conf[i++] = VIO_SCR_HEIGHT;
    conf[i++] = vc->height;
    conf[i++] = VIO_SCR_PIXELFORMAT;
    conf[i++] = (pbo) ? Surface_GetPixelFormat(pbo) : 0;
    conf[i]   = VIO_SCR_END;

    return EOK;
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

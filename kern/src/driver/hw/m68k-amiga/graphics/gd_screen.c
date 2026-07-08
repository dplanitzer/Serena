//
//  gd_screen.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd.h"
#include <hal/irq.h>


static int _get_config_value(const intptr_t* _Nonnull config, int key, intptr_t def)
{
    while (*config != SCREEN_CONF_END) {
        if (*config == key) {
            return *(config + 1);
        }
        config += 2;
    }

    return def;
}

// Parses the given 'icfg' in order to get a CLUT that is suitable for the
// screen configuration.
static errno_t _get_clut_from_config(const intptr_t* _Nonnull icfg, ColorTable* _Nullable * _Nonnull pOutClut, bool* _Nonnull pOutCreated)
{
    decl_try_err();
    ColorTable* clut = NULL;
    const int clut_id = _get_config_value(icfg, SCREEN_CONF_CLUT, -1);
    
    if (clut_id != -1) {
        clut = ColorTable_GetForId(clut_id);
        if (clut == NULL) {
            return EINVAL;
        }

        if (clut->entryCount != COLOR_COUNT) {
            return ENOTSUP;
        }

        *pOutCreated = false;
    }
    else {
        err = ColorTable_Create(COLOR_COUNT, kRGBColor32_Black, &clut);
        if (err != EOK) {
            return err;
        }

        *pOutCreated = true;
    }

    *pOutClut = clut;
    return EOK;
}

// Parses the given 'icfg' in order to get a surface that can be used as a
// framebuffer for the screen configuration.
static errno_t _get_framebuffer_from_config(const intptr_t* _Nonnull icfg, Surface* _Nullable * _Nonnull pOutSurface, const video_conf_t* _Nullable * _Nonnull pOutVc, bool* _Nonnull pOutCreated)
{
    decl_try_err();
    Surface* fb = NULL;
    const video_conf_t* vc = NULL;
    const fb_id = _get_config_value(icfg, SCREEN_CONF_FRAMEBUFFER, -1);

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
        const int w = _get_config_value(icfg, SCREEN_CONF_WIDTH, 0);
        const int h = _get_config_value(icfg, SCREEN_CONF_HEIGHT, 0);
        const int fmt = _get_config_value(icfg, SCREEN_CONF_PIXELFORMAT, 0);

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
    Surface* fb = NULL;
    bool bFbCreated = false;
    ColorTable* clut = NULL;
    bool bClutCreated = false;
    const video_conf_t* vc = NULL;
    copper_prog_t prog = NULL;
    

    // Compile the Copper program(s) for the new screen
    if (icfg) {
        try(_get_clut_from_config(icfg, &clut, &bClutCreated));
        try(_get_framebuffer_from_config(icfg, &fb, &vc, &bFbCreated));

        try(create_screen_copper_prog(vc, fb, clut, &prog));
    }
    else {
        try(create_null_copper_prog(&prog));
    } 


    // Schedule the new Copper program and wait until the new program is running
    // and the previous one has been retired. It's save to deallocate the old
    // framebuffer once the old program has stopped running.
    copper_schedule(prog, COPFLAG_WAIT_RUNNING);

catch:
    if (err != EOK) {
        if (bFbCreated) {
            Surface_DelRef(fb);
        }
        if (bClutCreated) {
            ColorTable_DelRef(clut);
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

    // SCREEN_CONF_FRAMEBUFFER
    // SCREEN_CONF_CLUT (if the pixel format is one of the indirect formats)
    // SCREEN_CONF_WIDTH
    // SCREEN_CONF_HEIGHT
    // SCREEN_CONF_PIXELFORMAT
    // SCREEN_CONF_END
    if (bufsiz < 11) {
        return ERANGE;
    }

    const unsigned sim = irq_set_mask(IRQ_MASK_VBLANK);
    const video_conf_t* vc = g_copper_running_prog->video_conf;
    Surface* fb = (Surface*)g_copper_running_prog->res.fb;
    ColorTable* clut = (ColorTable*)g_copper_running_prog->res.clut;
    irq_restore_mask(sim);

    conf[i++] = SCREEN_CONF_FRAMEBUFFER;
    conf[i++] = (fb) ? Surface_GetId(fb) : 0;
    conf[i++] = SCREEN_CONF_CLUT;
    conf[i++] = (clut) ? ColorTable_GetId(clut) : 0;
    conf[i++] = SCREEN_CONF_WIDTH;
    conf[i++] = vc->width;
    conf[i++] = SCREEN_CONF_HEIGHT;
    conf[i++] = vc->height;
    conf[i++] = SCREEN_CONF_PIXELFORMAT;
    conf[i++] = (fb) ? Surface_GetPixelFormat(fb) : 0;
    conf[i]   = SCREEN_CONF_END;

    return EOK;
}

errno_t gdSetScreenClutEntries(size_t idx, size_t count, const color_rgb32_t* _Nonnull entries)
{
    ColorTable* clut = (ColorTable*)g_copper_running_prog->res.clut;

    if (clut) {
        return ColorTable_SetEntries(clut, idx, count, entries);
    }
    else {
        return EINVAL;
    }
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

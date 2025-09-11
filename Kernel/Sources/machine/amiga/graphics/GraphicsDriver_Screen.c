//
//  GraphicsDriver_Screen.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/31/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "GraphicsDriverPriv.h"
#include "copper.h"
#include <machine/irq.h>


static int _get_config_value(const int* _Nonnull config, int key, int def)
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
static errno_t _get_clut_from_config(GraphicsDriverRef _Nonnull self, const int* _Nonnull icfg, ColorTable* _Nullable * _Nonnull pOutClut, bool* _Nonnull pOutCreated)
{
    decl_try_err();
    ColorTable* clut = NULL;
    const int clut_id = _get_config_value(icfg, SCREEN_CONF_CLUT, -1);
    
    if (clut_id != -1) {
        clut = _GraphicsDriver_GetClutForId(self, clut_id);
        if (clut == NULL) {
            return EINVAL;
        }

        if (clut->entryCount != COLOR_COUNT) {
            return ENOTSUP;
        }

        *pOutCreated = false;
    }
    else {
        err = _GraphicsDriver_CreateCLUT(self, COLOR_COUNT, kRGBColor32_Black, &clut);
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
static errno_t _get_framebuffer_from_config(GraphicsDriverRef _Nonnull self, const int* _Nonnull icfg, Surface* _Nullable * _Nonnull pOutSurface, const video_conf_t* _Nullable * _Nonnull pOutVc, bool* _Nonnull pOutCreated)
{
    decl_try_err();
    Surface* fb = NULL;
    const video_conf_t* vc = NULL;
    const fb_id = _get_config_value(icfg, SCREEN_CONF_FRAMEBUFFER, -1);

    if (fb_id != -1) {
        fb = _GraphicsDriver_GetSurfaceForId(self, fb_id);
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

        err = _GraphicsDriver_CreateSurface2d(self, w, h, fmt, &fb);
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
static errno_t GraphicsDriver_SetScreenConfig_Locked(GraphicsDriverRef _Nonnull _Locked self, const int* _Nullable icfg)
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
        try(_get_clut_from_config(self, icfg, &clut, &bClutCreated));
        try(_get_framebuffer_from_config(self, icfg, &fb, &vc, &bFbCreated));

        try(GraphicsDriver_CreateScreenCopperProg(self, vc, fb, clut, &prog));
    }
    else {
        try(GraphicsDriver_CreateNullCopperProg(self, &prog));
    } 


    // Schedule the new Copper program and wait until the new program is running
    // and the previous one has been retired. It's save to deallocate the old
    // framebuffer once the old program has stopped running.
    copper_schedule(prog, COPFLAG_WAIT_RUNNING);

catch:
    if (err != EOK) {
        if (bFbCreated) {
            _GraphicsDriver_DestroyGObj(self, fb);
        }
        if (bClutCreated) {
            _GraphicsDriver_DestroyGObj(self, clut);
        }
    }

    return err;
}

errno_t GraphicsDriver_SetScreenConfig(GraphicsDriverRef _Nonnull self, const int* _Nullable config)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    err = GraphicsDriver_SetScreenConfig_Locked(self, config);
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_GetScreenConfig(GraphicsDriverRef _Nonnull self, int* _Nonnull config, size_t bufsiz)
{
    decl_try_err();
    size_t i = 0;

    mtx_lock(&self->io_mtx);
    if (bufsiz == 0) {
        throw(EINVAL);
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
    irq_set_mask(sim);

    config[i++] = SCREEN_CONF_FRAMEBUFFER;
    config[i++] = (fb) ? GObject_GetId(fb) : 0;
    config[i++] = SCREEN_CONF_CLUT;
    config[i++] = (clut) ? GObject_GetId(clut) : 0;
    config[i++] = SCREEN_CONF_WIDTH;
    config[i++] = vc->width;
    config[i++] = SCREEN_CONF_HEIGHT;
    config[i++] = vc->height;
    config[i++] = SCREEN_CONF_PIXELFORMAT;
    config[i++] = (fb) ? Surface_GetPixelFormat(fb) : 0;
    config[i]   = SCREEN_CONF_END;

catch:
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_SetScreenCLUTEntries(GraphicsDriverRef _Nonnull self, size_t idx, size_t count, const RGBColor32* _Nonnull entries)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    ColorTable* clut = (ColorTable*)g_copper_running_prog->res.clut;

    if (clut) {
        err = ColorTable_SetEntries(clut, idx, count, entries);
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

void GraphicsDriver_GetScreenSize(GraphicsDriverRef _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight)
{
    const video_conf_t* vc = g_copper_running_prog->video_conf;

    *pOutWidth = vc->width;
    *pOutHeight = vc->height;
}

void GraphicsDriver_SetScreenConfigObserver(GraphicsDriverRef _Nonnull self, vcpu_t _Nullable vp, int signo)
{
    mtx_lock(&self->io_mtx);
    self->screenConfigObserver = vp;
    self->screenConfigObserverSignal = signo;
    mtx_unlock(&self->io_mtx);
}


void GraphicsDriver_SetLightPenEnabled(GraphicsDriverRef _Nonnull self, bool enabled)
{
    mtx_lock(&self->io_mtx);
    if (self->flags.isLightPenEnabled != enabled) {
        self->flags.isLightPenEnabled = enabled;

        copper_prog_t prog = _GraphicsDriver_GetEditableCopperProg(self);
        if (prog) {
            copper_prog_set_lp_enabled(prog, enabled);
            copper_schedule(prog, 0);
        }
    }
    mtx_unlock(&self->io_mtx);
}

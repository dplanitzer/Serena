//
//  GraphicsDriver_Screen.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/31/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "GraphicsDriverPriv.h"
#include "copper.h"
#include "copper_comp.h"

typedef struct screen_conf {
    Surface* _Nullable      fb;
    ColorTable* _Nullable   clut;
    int                     fps;
} screen_conf_t;


#define MAX_PIXEL_FORMATS   5
typedef struct hw_conf {
    int16_t     width;
    int16_t     height;
    int8_t      fps;
    int8_t      pixelFormatCount;   // Number of supported pixel formats
    PixelFormat pixelFormat[MAX_PIXEL_FORMATS];
} hw_conf_t;


#define NUM_HW_CONFS  8
static const hw_conf_t g_avail_hw_confs[NUM_HW_CONFS] = {
// NTSC 320x200 60fps
{320, 200, 60,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}
},
// NTSC 640x200 60fps
{640, 200, 60,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}
},
// NTSC 320x400 30fps (interlaced)
{320, 400, 30,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}
},
// NTSC 640x400 30fps (interlaced)
{640, 400, 30,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}
},
// PAL 320x256 50fps
{320, 256, 50,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}
},
// PAL 640x256 50fps
{640, 256, 50,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}
},
// PAL 320x512 25fps (interlaced)
{320, 512, 25,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}
},
// PAL 640x512 25fps (interlaced)
{640, 512, 25,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}
},
};


static void _make_copper_params(GraphicsDriverRef _Nonnull self, const hw_conf_t* hwc, Surface* _Nonnull fb, ColorTable* _Nonnull clut, copper_params_t* _Nonnull cp)
{
    cp->fb = fb;
    cp->clut = clut;
    cp->sprdma = self->spriteDmaPtr;
    cp->isHires = (hwc->width > MAX_LORES_WIDTH) ? true : false;
    cp->isLace = (hwc->height > MAX_PAL_HEIGHT) ? true : false;
    cp->isPal = (hwc->fps == 25 || hwc->fps == 50) ? true : false;
    cp->isLightPenEnabled = self->flags.isLightPenEnabled;
}

// Creates the even and odd field Copper programs for the given screen. There will
// always be at least an odd field program. The even field program will only exist
// for an interlaced screen.
static errno_t create_copper_prog(GraphicsDriverRef _Nonnull self, const hw_conf_t* _Nonnull hwc, Surface* _Nonnull srf, ColorTable* _Nonnull clut, copper_prog_t _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    copper_params_t params;
    copper_prog_t prog = NULL;
    copper_instr_t* ip;

    _make_copper_params(self, hwc, srf, clut, &params);
    
    const size_t instrCount = copper_comp_calclength(&params);

    try(copper_prog_create(instrCount, &prog));
    prog->odd_entry = prog->prog;
    prog->even_entry = NULL;
    
    ip = prog->prog;
    ip = copper_comp_compile(ip, &params, true);

    if (params.isLace) {
        prog->even_entry = ip;
        ip = copper_comp_compile(ip, &params, false);
    }

    self->hDiwStart = params.isPal ? DIW_PAL_HSTART : DIW_NTSC_HSTART;
    self->vDiwStart = params.isPal ? DIW_PAL_VSTART : DIW_NTSC_VSTART;
    self->hSprScale = params.isHires ? 0x01 : 0x00;
    self->vSprScale = params.isLace ? 0x01 : 0x00;

catch:
    *pOutProg = prog;

    return err;
}


static int _get_config_value(const int* _Nonnull config, int key, int def)
{
    while (*config != SCREEN_CONFIG_END) {
        if (*config == key) {
            return *(config + 1);
        }
        config++;
    }

    return def;
}

// Parses the given 'icfg' into a screen configuration structure and returns it
// in 'screenConf'.
static void _parse_screen_conf(GraphicsDriverRef _Nonnull self, const int* _Nonnull icfg, screen_conf_t* _Nonnull screenConf)
{
    const fb_id = _get_config_value(icfg, SCREEN_CONFIG_FRAMEBUFFER, -1);
    screenConf->fb = _GraphicsDriver_GetSurfaceForId(self, fb_id);


    const int clut_id = _get_config_value(icfg, SCREEN_CONFIG_CLUT, 0);
    screenConf->clut = _GraphicsDriver_GetClutForId(self, clut_id);


    screenConf->fps = _get_config_value(icfg, SCREEN_CONFIG_FPS, 0);
}

// Checks the consistency of the given screen configuration. Note that this does
// not check whether there's a corresponding hardware configuration  or not. This
// only checks the consistency of the screen configuration parameters (eg that
// framebuffer and CLUT related parameters are compatible with each other).
static bool _check_screen_conf(const screen_conf_t* _Nonnull conf)
{
    // Framebuffer CLUT must always provide entries for all color registers so
    // that we can specify sprite colors without issues.
    if (conf->clut && conf->clut->entryCount != 32) {
        return false;
    }

    return true;
}

// Looks up the hardware configuration that corresponds to the given screen
// configuration.
static const hw_conf_t* _Nullable _get_matching_hw_conf(const screen_conf_t* _Nonnull conf)
{
    const int fbWidth = (conf->fb) ? Surface_GetWidth(conf->fb) : 0;
    const int fbHeight = (conf->fb) ? Surface_GetHeight(conf->fb) : 0;
    const PixelFormat fbPixFmt = (conf->fb) ? Surface_GetPixelFormat(conf->fb) : 0;

    for (size_t i = 0; i < NUM_HW_CONFS; i++) {
        const hw_conf_t* hwc = &g_avail_hw_confs[i];

        if (hwc->width == fbWidth
            && hwc->height == fbHeight
            && hwc->fps == conf->fps) {
            for (int8_t i = 0; i < MAX_PIXEL_FORMATS; i++) {
                if (hwc->pixelFormat[i] == fbPixFmt) {
                    return hwc;
                }
            }
        }
    }

    return NULL;
}


// Sets the given screen as the current screen on the graphics driver. All graphics
// command apply to this new screen once this function has returned.
static errno_t GraphicsDriver_SetScreenConfig_Locked(GraphicsDriverRef _Nonnull _Locked self, const int* _Nullable icfg)
{
    decl_try_err();
    Surface* oldFb = self->fb;
    ColorTable* oldClut = self->clut;
    copper_prog_t prog = NULL;
    

    // Compile the Copper program(s) for the new screen
    if (icfg) {
        screen_conf_t conf;
        copper_params_t cp;
        const hw_conf_t* hwc;

        _parse_screen_conf(self, icfg, &conf);

        if (!_check_screen_conf(&conf)) {
            return ENOTSUP;
        }
        if ((hwc = _get_matching_hw_conf(&conf)) == NULL) {
            return ENOTSUP;
        }


        err = create_copper_prog(self, hwc, conf.fb, conf.clut, &prog);
        if (err != EOK) {
            return err;
        }

        self->hwc = hwc;
        self->fb = conf.fb;
        self->clut = conf.clut;
        Surface_BeginUse(conf.fb);
        ColorTable_BeginUse(conf.clut);
    }
    else {
        err = copper_comp_create_null_prog(self->nullSpriteData, &prog);
        if (err != EOK) {
            return err;
        }
        
        self->hDiwStart = 0;
        self->vDiwStart = 0;
        self->hSprScale = 0;
        self->vSprScale = 0;

        self->hwc = NULL;
        self->fb = NULL;
        self->clut = NULL;
    } 


    // Schedule the new Copper program and wait until the new program is running
    // and the previous one has been retired. It's save to deallocate the old
    // framebuffer once the old program has stopped running.
    copper_schedule(prog, COPFLAG_WAIT_RUNNING);


    // Free the old screen
    if (oldClut) {
        ColorTable_EndUse(oldClut);
        if (!ColorTable_IsUsed(oldClut)) {
            ColorTable_Destroy(oldClut);
        }
    }
    if (oldFb) {
        Surface_EndUse(oldFb);
        if (!Surface_IsUsed(oldFb)) {
            Surface_Destroy(oldFb);
        }
    }

    return EOK;
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

    mtx_lock(&self->io_mtx);
    if (bufsiz == 0) {
        throw(EINVAL);
    }

    //XXX implement me for real
    config[0] = SCREEN_CONFIG_END;

catch:
    mtx_unlock(&self->io_mtx);
    return err;
}

void GraphicsDriver_GetScreenSize(GraphicsDriverRef _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight)
{
    mtx_lock(&self->io_mtx);
    if (self->fb) {
        *pOutWidth = Surface_GetWidth(self->fb);
        *pOutHeight = Surface_GetHeight(self->fb);
    }
    else {
        *pOutWidth = 0;
        *pOutHeight = 0;
    }
    mtx_unlock(&self->io_mtx);
}

// Triggers an update of the display so that it accurately reflects the current
// display configuration.
errno_t GraphicsDriver_UpdateDisplay(GraphicsDriverRef _Nonnull self)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);

    if (self->flags.isNewCopperProgNeeded) {
        copper_prog_t prog = NULL;

        err = create_copper_prog(self, self->hwc, self->fb, self->clut, &prog);
        if (err == EOK) {
            copper_schedule(prog, 0);
            self->flags.isNewCopperProgNeeded = 0;
        }
    }

    mtx_unlock(&self->io_mtx);
    return err;
}

//
//  gd_framebuffer.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd_priv.h"

static vcpu_t _Nullable     g_screen_conf_observer;
static int                  g_screen_conf_signal;
uint16_t                    g_clut[CLUT_SIZE];
uint8_t                     g_clut_size = CLUT_SIZE;
static gd_display_mode_t    g_cur_display_mode;
static gd_display_params_t  g_cur_display_params;
const video_conf_t*         g_cur_video_config;
Surface*                    g_cur_front_buffer;


errno_t gdClut(size_t idx, size_t count, const gd_rgb32_t* _Nonnull entries)
{
    if (idx + count > g_clut_size) {
        return EINVAL;
    }


    if (count > 0) {
        for (size_t i = 0; i < count; i++) {
            const gd_rgb32_t color = entries[i];
            const uint16_t r = GD_RGB32_RED(color);
            const uint16_t g = GD_RGB32_GREEN(color);
            const uint16_t b = GD_RGB32_BLUE(color);

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

errno_t gdGetClut(size_t idx, size_t count, gd_rgb32_t* _Nonnull entries)
{
    if (idx + count > g_clut_size) {
        return EINVAL;
    }


    for (size_t i = 0; i < count; i++) {
        const uint16_t color = g_clut[idx + i];
        const uint16_t r = ((color >> 8) & 0x0f) << 4;
        const uint16_t g = ((color >> 4) & 0x0f) << 4;
        const uint16_t b = (color & 0x0f) << 4;

        entries[i] = GD_RGB32_MAKE(r, g, b);
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

errno_t gdDisplayMode(const gd_display_mode_t* _Nonnull mode, const gd_display_params_t* _Nullable params, int op)
{
    decl_try_err();
    Surface* front_buf;
    copper_prog_t prog = NULL;
    
    switch (op) {
        case GD_APPLY:
        case GD_CHECK:
        case 2: //XXX revisit this
            break;

        default:
            return EINVAL;
    }


    // Validate the display mode and parameters
    const video_conf_t* vc = get_matching_video_conf(mode, params);
    if (vc == NULL) {
        return ENOTSUP; //XXX ENOMATCH?
    }


    // We're done if this is just a check
    if (op == GD_CHECK) {
        return EOK;
    }


    // Allocate the framebuffer
    err = Surface_Create(mode->width, mode->height, mode->pixelFormat, &front_buf);
    if (err != EOK) {
        return err;
    }


    // Clear the framebuffer to black if requested
    //XXX revisit this
    if (op == 2) {
        Surface_ClearPixels(front_buf);
    }

    
    // Compile the Copper program(s) for the new framebuffer
    err = create_screen_copper_prog(vc, front_buf, &prog);
    if (err != EOK) {
        Surface_DelRef(front_buf);
        return err;
    }


    // Schedule the new Copper program and wait until the new program is running
    // and the previous one has been retired. It's save to deallocate the old
    // framebuffer once the old program has stopped running.
    // Note that this call automatically unschedules and retires any pending
    // Copper program.
    copper_schedule(prog, COPFLAG_WAIT_RUNNING);

    if (g_cur_front_buffer) {
        Surface_DelRef(g_cur_front_buffer);
        g_cur_front_buffer = NULL;
    }
    g_cur_display_mode = *mode;
    g_cur_display_params = (params) ? *params : (gd_display_params_t){0};
    g_cur_front_buffer = front_buf;
    g_cur_video_config = vc;



    if (g_screen_conf_observer) {
        vcpu_send_signal(g_screen_conf_observer, g_screen_conf_signal);
    }


catch:
    return err;
}

errno_t gdGetDisplayInfo(int flavor, gd_display_info_ref_t _Nonnull info)
{
    switch (flavor) {
        case GD_DISPLAY_MODE: {
            gd_display_mode_t* ip = info;

            *ip = g_cur_display_mode;
            return EOK;
        }

        case GD_DISPLAY_PARAMS: {
            gd_display_params_t* ip = info;

            *ip = g_cur_display_params;
            return EOK;
        }

        case GD_DISPLAY_BUFFERS: {
            gd_display_buffers_t* ip = info;

            *ip = (gd_display_buffers_t){0};
            ip->front_left = (g_cur_front_buffer) ? g_cur_front_buffer->id : 0;
            return EOK;
        }

        default:
            return EINVAL;
    }
}

errno_t gdEnumDisplayModes(int index, gd_display_mode_t* _Nonnull pOutMode)
{
    if (index < 0 || index >= NUM_VIDEO_CONFIGS) {
        return EINVAL;
    }

    const video_conf_t* vc = &g_video_conf[index];
    pOutMode->width = vc->width;
    pOutMode->height = vc->height;
    pOutMode->refreshRate = vc->refreshRate;
    pOutMode->pixelFormat = vc->pixelFormat;
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

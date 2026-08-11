//
//  IOGraphicsHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "IOGraphicsHandler.h"
#include <driver/hw/m68k-amiga/graphics/AGADriver.h>


errno_t IOGraphicsHandler_Create(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    return IODriverHandler_Create(class(IOGraphicsHandler), FD_TYPE_DEVICE, ip, flags, pOutHandler);
}

errno_t IOGraphicsHandler_control(struct IOGraphicsHandler* _Nonnull self, int cmd, va_list ap)
{
    AGADriverRef drv = IODriverHandler_GetDriver(self);

    switch (cmd) {
        //
        // Image
        //

        case GDC_CREATE_IMAGE: {
            const int width = va_arg(ap, int);
            const int height = va_arg(ap, int);
            const gd_pixfmt_t fmt = va_arg(ap, gd_pixfmt_t);
            int* img_id = va_arg(ap, int*);

            return AGADriver_CreateImage(drv, width, height, fmt, img_id);
        }

        case GDC_DESTROY_IMAGE: {
            int img_id = va_arg(ap, int);

            return AGADriver_DestroyImage(drv, img_id);
        }

        case GDC_GET_IMAGE_INFO: {
            int img_id = va_arg(ap, int);
            gd_image_info_t* si = va_arg(ap, gd_image_info_t*);

            return AGADriver_GetImageInfo(drv, img_id, si);
        }

        case GDC_MAP_IMAGE: {
            int img_id = va_arg(ap, int);
            int mode = va_arg(ap, int);
            gd_image_data_t* sm = va_arg(ap, gd_image_data_t*);

            return AGADriver_MapImage(drv, img_id, mode, sm);
        }

        case GDC_UNMAP_IMAGE: {
            const int img_id = va_arg(ap, int);

            return AGADriver_UnmapImage(drv, img_id);
        }


        //
        // Command Buffer
        //
        
        case GDC_CREATE_CMDBUF: {
            size_t size = va_arg(ap, size_t);
            gd_cmdbuf_desc_t* desc = va_arg(ap, gd_cmdbuf_desc_t*);

            return AGADriver_CreateCommandBuffer(drv, size, desc);
        }

        case GDC_DESTROY_CMDBUF: {
            int cmdimg_id = va_arg(ap, int);

            return AGADriver_DestroyCommandBuffer(drv, cmdimg_id);
        }

        case GDC_SUBMIT_CMDBUF: {
            int queue_id = va_arg(ap, int);
            int cmdimg_id = va_arg(ap, int);

            return AGADriver_SubmitCommandBuffer(drv, queue_id, cmdimg_id);
        }


        //
        // Sprites
        //

        case GDC_ACQUIRE_SPRITES: {
            int basePri = va_arg(ap, int);
            size_t count = va_arg(ap, size_t);
            int* pOutSpriteIds = va_arg(ap, int*);

            return AGADriver_AcquireSprites(drv, basePri, count, pOutSpriteIds);
        }

        case GDC_RELEASE_SPRITES: {
            const int* spriteIds = va_arg(ap, const int*);
            size_t count = va_arg(ap, size_t);

            return AGADriver_ReleaseSprites(drv, spriteIds, count);
        }

        case GDC_SPRITE_CONSTRAINTS: {
            gd_sprite_constraints_t* cp = va_arg(ap, gd_sprite_constraints_t*);

            AGADriver_GetSpriteCaps(drv, cp);
            return EOK;
        }


        //
        // CLUT
        //

        case GDC_CLUT: {
            const size_t idx = va_arg(ap, size_t);
            const size_t count = va_arg(ap, size_t);
            const gd_rgb32_t* entries = va_arg(ap, const gd_rgb32_t*);

            return AGADriver_Clut(drv, idx, count, entries);
        }

        case GDC_GET_CLUT: {
            const size_t idx = va_arg(ap, size_t);
            const size_t count = va_arg(ap, size_t);
            gd_rgb32_t* entries = va_arg(ap, gd_rgb32_t*);

            return AGADriver_GetClut(drv, idx, count, entries);
        }

        case GDC_GET_CLUT_INFO: {
            gd_clut_info_t* info = va_arg(ap, gd_clut_info_t*);

            return AGADriver_GetClutInfo(drv, info);
        }


        //
        // Display
        //

        case GDC_DISPLAY_MODE: {
            const gd_display_mode_t* mode = va_arg(ap, const gd_display_mode_t*);
            const gd_display_params_t* params = va_arg(ap, const gd_display_params_t*);
            const int op = va_arg(ap, int);

            return AGADriver_DisplayMode(drv, mode, params, op);
        }

        case GDC_GET_DISPLAY_INFO: {
            const int flavor = va_arg(ap, int);
            gd_display_info_ref_t info = va_arg(ap, gd_display_info_ref_t);

            return AGADriver_GetDisplayInfo(drv, flavor, info);
        }

        case GDC_ENUM_DISPLAY_MODES: {
            const int index = va_arg(ap, int);
            gd_display_mode_t* mode = va_arg(ap, gd_display_mode_t*);

            return AGADriver_EnumDisplayModes(drv, index, mode);
        }


        //
        // Inherited Functions
        //
        default:
            return Handler_Super_Control(IOGraphicsHandler, cmd, ap);
    }
}


class_func_defs(IOGraphicsHandler, IODriverHandler,
override_func_def(control, IOGraphicsHandler, Handler)
);

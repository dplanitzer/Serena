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
        // Pixel Buffer
        //

        case GDC_CREATE_BUFFER: {
            const int width = va_arg(ap, int);
            const int height = va_arg(ap, int);
            const gd_pixfmt_t fmt = va_arg(ap, gd_pixfmt_t);
            int* buf_id = va_arg(ap, int*);

            return AGADriver_CreateBuffer(drv, width, height, fmt, buf_id);
        }

        case GDC_DESTROY_BUFFER: {
            int buf_id = va_arg(ap, int);

            return AGADriver_DestroyBuffer(drv, buf_id);
        }

        case GDC_BUFFER_INFO: {
            int buf_id = va_arg(ap, int);
            gd_buffer_info_t* si = va_arg(ap, gd_buffer_info_t*);

            return AGADriver_GetBufferInfo(drv, buf_id, si);
        }

        case GDC_MAP_BUFFER: {
            int buf_id = va_arg(ap, int);
            int mode = va_arg(ap, int);
            gd_buffer_data_t* sm = va_arg(ap, gd_buffer_data_t*);

            return AGADriver_MapBuffer(drv, buf_id, mode, sm);
        }

        case GDC_UNMAP_BUFFER: {
            const int buf_id = va_arg(ap, int);

            return AGADriver_UnmapBuffer(drv, buf_id);
        }

        case GDC_BUFFER_COMMANDS: {
            int buf_id = va_arg(ap, int);
            int cmdbuf_id = va_arg(ap, int);
            size_t offset = va_arg(ap, size_t);

            return AGADriver_BufferCommands(drv, buf_id, cmdbuf_id, offset);
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
            int cmdbuf_id = va_arg(ap, int);

            return AGADriver_DestroyCommandBuffer(drv, cmdbuf_id);
        }


        //
        // Sprites
        //

        case GDC_SPRITE_CAPS: {
            gd_sprite_caps_t* cp = va_arg(ap, gd_sprite_caps_t*);

            AGADriver_GetSpriteCaps(drv, cp);
            return EOK;
        }


        //
        // CLUT
        //

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

        case GDC_DISPLAY_COMMANDS: {
            int cmdbuf_id = va_arg(ap, int);
            size_t offset = va_arg(ap, size_t);

            return AGADriver_DisplayCommands(drv, cmdbuf_id, offset);
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

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

        case VIO_CMD_CREATE_BUFFER: {
            const int width = va_arg(ap, int);
            const int height = va_arg(ap, int);
            const vio_pixfmt_t fmt = va_arg(ap, vio_pixfmt_t);
            int* buf_id = va_arg(ap, int*);

            return AGADriver_CreateBuffer(drv, width, height, fmt, buf_id);
        }

        case VIO_CMD_DESTROY_BUFFER: {
            int buf_id = va_arg(ap, int);

            return AGADriver_DestroyBuffer(drv, buf_id);
        }

        case VIO_CMD_BUFFER_INFO: {
            int buf_id = va_arg(ap, int);
            vio_buffer_info_t* si = va_arg(ap, vio_buffer_info_t*);

            return AGADriver_GetBufferInfo(drv, buf_id, si);
        }

        case VIO_CMD_MAP_BUFFER: {
            int buf_id = va_arg(ap, int);
            int mode = va_arg(ap, int);
            vio_buffer_data_t* sm = va_arg(ap, vio_buffer_data_t*);

            return AGADriver_MapBuffer(drv, buf_id, mode, sm);
        }

        case VIO_CMD_UNMAP_BUFFER: {
            const int buf_id = va_arg(ap, int);

            return AGADriver_UnmapBuffer(drv, buf_id);
        }

        case VIO_CMD_BUFFER_COMMANDS: {
            int buf_id = va_arg(ap, int);
            int cmdbuf_id = va_arg(ap, int);
            size_t offset = va_arg(ap, size_t);

            return AGADriver_BufferCommands(drv, buf_id, cmdbuf_id, offset);
        }


        case VIO_CMD_CREATE_FRAMEBUFFER: {
            const size_t entryCount = va_arg(ap, size_t);
            int* fb_id = va_arg(ap, int*);

            return AGADriver_CreateFramebuffer(drv, entryCount, fb_id);
        }

        case VIO_CMD_DESTROY_FRAMEBUFFER: {
            int fb_id = va_arg(ap, int);

            return AGADriver_DestroyFramebuffer(drv, fb_id);
        }

        case VIO_CMD_ATTACH_BUFFER: {
            int fb_id = va_arg(ap, int);
            int buf_id = va_arg(ap, int);

            return AGADriver_AttachBuffer(drv, fb_id, buf_id);
        }


        //
        // Sprite
        //

        case VIO_CMD_SPRITE_CAPS: {
            vio_sprite_caps_t* cp = va_arg(ap, vio_sprite_caps_t*);

            AGADriver_GetSpriteCaps(drv, cp);
            return EOK;
        }


        case VIO_CMD_SET_CURRENT_FRAMEBUFFER: {
            const int fb_id = va_arg(ap, int);

            return AGADriver_SetCurrentFramebuffer(drv, fb_id);
        }

        case VIO_CMD_GET_CURRENT_FRAMEBUFFER: {
            int* p_fb_id = va_arg(ap, int*);

            *p_fb_id = AGADriver_GetCurrentFramebuffer(drv);
            return EOK;
        }


        //
        // Command Buffer
        //
        
        case VIO_CMD_CREATE_CMDBUF: {
            size_t size = va_arg(ap, size_t);
            vio_cmdbuf_desc_t* desc = va_arg(ap, vio_cmdbuf_desc_t*);

            return AGADriver_CreateCommandBuffer(drv, size, desc);
        }

        case VIO_CMD_DESTROY_CMDBUF: {
            int cmdbuf_id = va_arg(ap, int);

            return AGADriver_DestroyCommandBuffer(drv, cmdbuf_id);
        }


        //
        // Display
        //

        case VIO_CMD_GET_CLUT: {
            const size_t idx = va_arg(ap, size_t);
            const size_t count = va_arg(ap, size_t);
            vio_rgb32_t* entries = va_arg(ap, vio_rgb32_t*);

            return AGADriver_GetClut(drv, idx, count, entries);
        }

        case VIO_CMD_GET_CLUT_INFO: {
            gd_clut_info_t* info = va_arg(ap, gd_clut_info_t*);

            return AGADriver_GetClutInfo(drv, info);
        }

        case VIO_CMD_DISPLAY_COMMANDS: {
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

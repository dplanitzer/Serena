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

        case VIO_CMD_FRAMEBUFFER_INFO: {
            int fb_id = va_arg(ap, int);
            vio_clut_info_t* ci = va_arg(ap, vio_clut_info_t*);

            return AGADriver_GetFramebufferInfo(drv, fb_id, ci);
        }


        case VIO_CMD_SPRITE_CAPS: {
            vio_sprite_caps_t* cp = va_arg(ap, vio_sprite_caps_t*);

            AGADriver_GetSpriteCaps(drv, cp);
            return EOK;
        }


        case VIO_CMD_SET_SCREEN_CONFIG: {
            const intptr_t* cp = va_arg(ap, const intptr_t*);

            return AGADriver_SetScreenConfig(drv, cp);
        }

        case VIO_CMD_SCREEN_CONFIG: {
            intptr_t* cp = va_arg(ap, intptr_t*);
            size_t bufsiz = va_arg(ap, size_t);

            return AGADriver_GetScreenConfig(drv, cp, bufsiz);
        }

        case VIO_CMD_CREATE_CMDBUF: {
            size_t size = va_arg(ap, size_t);
            vio_cmdbuf_desc_t* desc = va_arg(ap, vio_cmdbuf_desc_t*);

            return AGADriver_CreateCommandBuffer(drv, size, desc);
        }

        case VIO_CMD_DESTROY_CMDBUF: {
            int cmdbuf_id = va_arg(ap, int);

            return AGADriver_DestroyCommandBuffer(drv, cmdbuf_id);
        }

        case VIO_CMD_EXEC_CMDBUF: {
            int cmdbuf_id = va_arg(ap, int);
            size_t offset = va_arg(ap, size_t);

            return AGADriver_ExecuteCommandBuffer(drv, cmdbuf_id, offset);
        }

        default:
            return Handler_Super_Control(IOGraphicsHandler, cmd, ap);
    }
}


class_func_defs(IOGraphicsHandler, IODriverHandler,
override_func_def(control, IOGraphicsHandler, Handler)
);

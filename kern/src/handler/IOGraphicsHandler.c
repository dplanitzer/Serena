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
        case IOCMD_FB_CREATE_SURFACE_2D: {
            const int width = va_arg(ap, int);
            const int height = va_arg(ap, int);
            const pixfmt_t fmt = va_arg(ap, pixfmt_t);
            int* hnd = va_arg(ap, int*);

            return AGADriver_CreateSurface2d(drv, width, height, fmt, hnd);
        }

        case IOCMD_FB_DESTROY_SURFACE: {
            int hnd = va_arg(ap, int);

            return AGADriver_DestroySurface(drv, hnd);
        }

        case IOCMD_FB_SURFACE_INFO: {
            int hnd = va_arg(ap, int);
            surface_info_t* si = va_arg(ap, surface_info_t*);

            return AGADriver_GetSurfaceInfo(drv, hnd, si);
        }

        case IOCMD_FB_MAP_SURFACE: {
            int hnd = va_arg(ap, int);
            int mode = va_arg(ap, int);
            surface_mapping_t* sm = va_arg(ap, surface_mapping_t*);

            return AGADriver_MapSurface(drv, hnd, mode, sm);
        }

        case IOCMD_FB_UNMAP_SURFACE: {
            const int hnd = va_arg(ap, int);

            return AGADriver_UnmapSurface(drv, hnd);
        }

        case IOCMD_FB_WRITE_PIXELS: {
            int hnd = va_arg(ap, int);
            const void* planes = va_arg(ap, const void*);
            size_t bytesPerRow = va_arg(ap, size_t);
            pixfmt_t format = va_arg(ap, pixfmt_t);

            return AGADriver_WritePixels(drv, hnd, planes, bytesPerRow, format);
        }

        case IOCMD_FB_CLEAR_PIXELS: {
            const int hnd = va_arg(ap, int);

            return AGADriver_ClearPixels(drv, hnd);
        }

        case IOCMD_FB_BIND_SURFACE: {
            const int target = va_arg(ap, int);
            const int id = va_arg(ap, int);

            return AGADriver_BindSurface(drv, target, id);
        }


        case IOCMD_FB_CREATE_CLUT: {
            const size_t entryCount = va_arg(ap, size_t);
            int* hnd = va_arg(ap, int*);

            return AGADriver_CreateCLUT(drv, entryCount, hnd);
        }

        case IOCMD_FB_DESTROY_CLUT: {
            int hnd = va_arg(ap, int);

            return AGADriver_DestroyCLUT(drv, hnd);
        }

        case IOCMD_FB_CLUT_INFO: {
            int hnd = va_arg(ap, int);
            clut_info_t* ci = va_arg(ap, clut_info_t*);

            return AGADriver_GetCLUTInfo(drv, hnd, ci);
        }

        case IOCMD_FB_SET_CLUT_ENTRIES: {
            const int hnd = va_arg(ap, int);
            const size_t idx = va_arg(ap, size_t);
            const size_t count = va_arg(ap, size_t);
            const color_rgb32_t* colors = va_arg(ap, const color_rgb32_t*);

            return AGADriver_SetCLUTEntries(drv, hnd, idx, count, colors);
        }


        case IOCMD_FB_SPRITE_CAPS: {
            sprite_caps_t* cp = va_arg(ap, sprite_caps_t*);

            AGADriver_GetSpriteCaps(drv, cp);
            return EOK;
        }

        case IOCMD_FB_SET_SPRITE_POS: {
            const int hnd = va_arg(ap, int);
            const int x = va_arg(ap, int);
            const int y = va_arg(ap, int);

            return AGADriver_SetSpritePosition(drv, hnd, x, y);
        }

        case IOCMD_FB_SET_SPRITE_VIS: {
            const int hnd = va_arg(ap, int);
            const bool flag = va_arg(ap, int);

            return AGADriver_SetSpriteVisible(drv, hnd, flag);
        }


        case IOCMD_FB_SET_SCREEN_CONFIG: {
            const intptr_t* cp = va_arg(ap, const intptr_t*);

            return AGADriver_SetScreenConfig(drv, cp);
        }

        case IOCMD_FB_SCREEN_CONFIG: {
            intptr_t* cp = va_arg(ap, intptr_t*);
            size_t bufsiz = va_arg(ap, size_t);

            return AGADriver_GetScreenConfig(drv, cp, bufsiz);
        }

        case IOCMD_FB_SET_SCREEN_CLUT_ENTRIES: {
            const size_t idx = va_arg(ap, size_t);
            const size_t count = va_arg(ap, size_t);
            const color_rgb32_t* colors = va_arg(ap, const color_rgb32_t*);

            return AGADriver_SetScreenCLUTEntries(drv, idx, count, colors);
        }

        default:
            return Handler_Super_Control(IOGraphicsHandler, cmd, ap);
    }
}


class_func_defs(IOGraphicsHandler, IODriverHandler,
override_func_def(control, IOGraphicsHandler, Handler)
);

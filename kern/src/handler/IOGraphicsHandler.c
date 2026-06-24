//
//  IOGraphicsHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "IOGraphicsHandler.h"
#include <driver/hw/m68k-amiga/graphics/GraphicsDriver.h>


errno_t IOGraphicsHandler_Create(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    return IODriverHandler_Create(class(IOGraphicsHandler), FD_TYPE_DRIVER, ip, flags, pOutHandler);
}

errno_t IOGraphicsHandler_control(struct IOGraphicsHandler* _Nonnull self, int cmd, va_list ap)
{
    GraphicsDriverRef drv = IODriverHandler_GetDriver(self);

    switch (cmd) {
        case kFBCommand_CreateSurface2d: {
            const int width = va_arg(ap, int);
            const int height = va_arg(ap, int);
            const pixfmt_t fmt = va_arg(ap, pixfmt_t);
            int* hnd = va_arg(ap, int*);

            return GraphicsDriver_CreateSurface2d(drv, width, height, fmt, hnd);
        }

        case kFBCommand_DestroySurface: {
            int hnd = va_arg(ap, int);

            return GraphicsDriver_DestroySurface(drv, hnd);
        }

        case kFBCommand_GetSurfaceInfo: {
            int hnd = va_arg(ap, int);
            surface_info_t* si = va_arg(ap, surface_info_t*);

            return GraphicsDriver_GetSurfaceInfo(drv, hnd, si);
        }

        case kFBCommand_MapSurface: {
            int hnd = va_arg(ap, int);
            int mode = va_arg(ap, int);
            surface_mapping_t* sm = va_arg(ap, surface_mapping_t*);

            return GraphicsDriver_MapSurface(drv, hnd, mode, sm);
        }

        case kFBCommand_UnmapSurface: {
            const int hnd = va_arg(ap, int);

            return GraphicsDriver_UnmapSurface(drv, hnd);
        }

        case kFBCommand_WritePixels: {
            int hnd = va_arg(ap, int);
            const void* planes = va_arg(ap, const void*);
            size_t bytesPerRow = va_arg(ap, size_t);
            pixfmt_t format = va_arg(ap, pixfmt_t);

            return GraphicsDriver_WritePixels(drv, hnd, planes, bytesPerRow, format);
        }

        case kFBCommand_ClearPixels: {
            const int hnd = va_arg(ap, int);

            return GraphicsDriver_ClearPixels(drv, hnd);
        }

        case kFBCommand_BindSurface: {
            const int target = va_arg(ap, int);
            const int id = va_arg(ap, int);

            return GraphicsDriver_BindSurface(drv, target, id);
        }


        case kFBCommand_CreateCLUT: {
            const size_t entryCount = va_arg(ap, size_t);
            int* hnd = va_arg(ap, int*);

            return GraphicsDriver_CreateCLUT(drv, entryCount, hnd);
        }

        case kFBCommand_DestroyCLUT: {
            int hnd = va_arg(ap, int);

            return GraphicsDriver_DestroyCLUT(drv, hnd);
        }

        case kFBCommand_GetCLUTInfo: {
            int hnd = va_arg(ap, int);
            clut_info_t* ci = va_arg(ap, clut_info_t*);

            return GraphicsDriver_GetCLUTInfo(drv, hnd, ci);
        }

        case kFBCommand_SetCLUTEntries: {
            const int hnd = va_arg(ap, int);
            const size_t idx = va_arg(ap, size_t);
            const size_t count = va_arg(ap, size_t);
            const color_rgb32_t* colors = va_arg(ap, const color_rgb32_t*);

            return GraphicsDriver_SetCLUTEntries(drv, hnd, idx, count, colors);
        }


        case kFBCommand_GetSpriteCaps: {
            sprite_caps_t* cp = va_arg(ap, sprite_caps_t*);

            GraphicsDriver_GetSpriteCaps(drv, cp);
            return EOK;
        }

        case kFBCommand_SetSpritePosition: {
            const int hnd = va_arg(ap, int);
            const int x = va_arg(ap, int);
            const int y = va_arg(ap, int);

            return GraphicsDriver_SetSpritePosition(drv, hnd, x, y);
        }

        case kFBCommand_SetSpriteVisible: {
            const int hnd = va_arg(ap, int);
            const bool flag = va_arg(ap, int);

            return GraphicsDriver_SetSpriteVisible(drv, hnd, flag);
        }


        case kFBCommand_SetScreenConfig: {
            const intptr_t* cp = va_arg(ap, const intptr_t*);

            return GraphicsDriver_SetScreenConfig(drv, cp);
        }

        case kFBCommand_GetScreenConfig: {
            intptr_t* cp = va_arg(ap, intptr_t*);
            size_t bufsiz = va_arg(ap, size_t);

            return GraphicsDriver_GetScreenConfig(drv, cp, bufsiz);
        }

        case kFBCommand_SetScreenCLUTEntries: {
            const size_t idx = va_arg(ap, size_t);
            const size_t count = va_arg(ap, size_t);
            const color_rgb32_t* colors = va_arg(ap, const color_rgb32_t*);

            return GraphicsDriver_SetScreenCLUTEntries(drv, idx, count, colors);
        }

        default:
            return Handler_Super_Control(IOGraphicsHandler, cmd, ap);
    }
}


class_func_defs(IOGraphicsHandler, IODriverHandler,
override_func_def(control, IOGraphicsHandler, Handler)
);

//
//  Surface.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Surface.h"
#include "video_conf.h"
#include <ext/math.h>
#include <string.h>
#include <kern/kalloc.h>

static int      g_next_surface_id = 1;
static deque_t  g_surface_table;


static errno_t _alloc_single_plane(Surface* _Nonnull self)
{
    decl_try_err();
    size_t nbytes;

    switch (self->pixelFormat) {
        case VIO_RGB_SPRITE_2:
            // sprxctl, sprxctl, (plane0, plane1)..., 0, 0
            nbytes = 2*sizeof(uint16_t) + (2*self->height*self->bytesPerRow) + 2*sizeof(uint16_t);
            break;

        default:
            nbytes = self->bytesPerRow * self->height;
            break;
    }

    try(kalloc_options(nbytes, KALLOC_OPTION_UNIFIED, (void**) &self->plane[0]));

    if (self->pixelFormat == VIO_RGB_SPRITE_2) {
        uint16_t* p = (uint16_t*)self->plane[0];

        p[0] = 0;
        p[1] = 0;
        p[2 + (self->height << 1) + 0] = 0;
        p[2 + (self->height << 1) + 1] = 0;
    }

catch:
    return err;
}

static errno_t _alloc_multi_plane(Surface* _Nonnull self)
{
    decl_try_err();
    const size_t bytesPerPlane = self->bytesPerRow * self->height;
    const size_t bytesPerClusteredPlane = __Ceil_PowerOf2(bytesPerPlane, 4);
    const size_t clusteredSize = self->planeCount * bytesPerClusteredPlane;

    // Allocate the planes. Note that we try to cluster the planes whenever possible.
    // This means that we allocate a single contiguous memory range big enough to
    // hold all planes. We only allocate independent planes if we're not able to
    // allocate a big enough contiguous memory region because DMA memory has become
    // too fragmented to pull this off. Individual planes in a clustered planes
    // configuration are aligned on an 4 byte boundary.

    if (kalloc_options(clusteredSize, KALLOC_OPTION_UNIFIED, (void**) &self->plane[0])) {
        for (int8_t i = 1; i < self->planeCount; i++) {
            self->plane[i] = self->plane[i - 1] + bytesPerClusteredPlane;
        }
        self->flags |= kSurfaceFlag_ClusteredPlanes;
    }
    else {
        for (int8_t i = 0; i < self->planeCount; i++) {
            err = kalloc_options(bytesPerPlane, KALLOC_OPTION_UNIFIED, (void**) &self->plane[i]);
            if (err != EOK) {
                break;
            }
        }
    }

    return err;
}

static void _destroy(Surface* _Nonnull self)
{
    if ((self->flags & kSurfaceFlag_IsRegistered) != 0) {
        deque_remove(&g_surface_table, &self->chain);
    }

    if ((self->flags & kSurfaceFlag_ClusteredPlanes) != 0) {
        kfree(self->plane[0]);
    }
    else {
        for (int i = 0; i < self->planeCount; i++) {
            kfree(self->plane[i]);
        }
    }

    kfree(self);
}


// Allocates a new surface with the given pixel width and height and pixel
// format.
// \param width the width in pixels
// \param height the height in pixels
// \param pixelFormat the pixel format
// \return the surface; NULL on failure
errno_t Surface_Create(int width, int height, vio_pixfmt_t pixelFormat, Surface* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Surface* self;
    
    if (width <= 0 || height <= 0) {
        return EINVAL;
    }


    try(kalloc_cleared(sizeof(Surface), (void**) &self));
    
    self->id = g_next_surface_id++;
    self->refCount = 1;
    self->pixelFormat = pixelFormat;
    self->width = width;
    self->height = height;
    self->bytesPerRow = ((width + 15) >> 4) << 1;       // Must be a multiple of at least words (16bits)
    self->planeCount = PixelFormat_GetPlaneCount(pixelFormat);


    if (self->planeCount == 1) {
        try(_alloc_single_plane(self));
    }
    else {
        try(_alloc_multi_plane(self));
    }
    

    deque_add_first(&g_surface_table, &self->chain);
    self->flags |= kSurfaceFlag_IsRegistered;

    *pOutSelf = self;
    return EOK;
    
catch:
    _destroy(self);
    *pOutSelf = NULL;
    return err;
}

errno_t Surface_CreateNullSprite(Surface* _Nullable * _Nonnull pOutSelf)
{
    Surface* self;
    const errno_t err = Surface_Create(16, 1, VIO_RGB_SPRITE_2, &self);

    if (err == EOK) {
        uint16_t* pp = (uint16_t*)self->plane[0];

        pp[0] = 0x1905;
        pp[1] = 0x1a00;
        pp[2] = 0;
        pp[3] = 0;
        pp[4] = 0;
        pp[5] = 0;
    }

    *pOutSelf = self;
    return err;
}

void Surface_DelRef(Surface* _Nullable self)
{
    if (self) {
        self->refCount--;
        if (self->refCount == 0) {
            _destroy(self);
        }
    }
}

Surface* _Nullable Surface_GetForId(int id)
{
    deque_for_each(&g_surface_table, Surface, it,
        if (it->id == id) {
            return it;
        }
    )
    return NULL;
}

errno_t Surface_WritePixels(Surface* _Nonnull self, const void* _Nonnull planes[], size_t bytesPerRow, vio_pixfmt_t format)
{
    if (self->pixelFormat == VIO_RGB_SPRITE_2 && format == VIO_COLOR_INDEX2) {
        const uint8_t* sp0 = planes[0];
        const uint8_t* sp1 = planes[1];
        uint16_t* pp = (uint16_t*)self->plane[0];
        uint16_t* dp = &pp[2];

        for (int y = 0; y < self->height; y++) {
            *dp++ = *(uint16_t*)sp0; sp0 += bytesPerRow;
            *dp++ = *(uint16_t*)sp1; sp1 += bytesPerRow;
        }

        return EOK;
    }
    else if (self->pixelFormat == format) {
        for (int8_t p = 0; p < self->planeCount; p++) {
            const uint8_t* sp = planes[p];
            uint8_t* dp = self->plane[p];

            for (int y = 0; y < self->height; y++) {
                memcpy(dp, sp, self->width >> 3);
                dp += self->bytesPerRow;
                sp += bytesPerRow;
            }
        }

        return EOK;
    }
    else {
        return ENOTSUP;
    }
}

errno_t Surface_ClearPixels(Surface* _Nonnull self)
{
    if (self->pixelFormat == VIO_RGB_SPRITE_2) {
        uint16_t* pp = (uint16_t*)self->plane[0];
        uint16_t* dp = &pp[2];

        for (int y = 0; y < self->height; y++) {
            *dp++ = 0;
            *dp++ = 0;
        }

        return EOK;
    }
    else {
        for (int8_t p = 0; p < self->planeCount; p++) {
            memset(self->plane[p], 0, self->bytesPerRow * self->height);
        }

        return EOK;
    }
}

//
//  Surface.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Surface.h"
#include <kern/kalloc.h>


// Returns how many planes are needed to store a pixel in the given pixel format.
// Returns 1 if the pixel format is a direct pixel format.
static int8_t PixelFormat_GetPlaneCount(PixelFormat format)
{
    switch (format) {
        case kPixelFormat_RGB_Indexed1:
        case kPixelFormat_RGB_Indexed2:
        case kPixelFormat_RGB_Indexed3:
        case kPixelFormat_RGB_Indexed4:
        case kPixelFormat_RGB_Indexed5:
            return format + 1;

        case kPixelFormat_RGB_Sprite2:
            return 1;

        default:
            return 1;
    }
}


static errno_t _alloc_single_plane(Surface* _Nonnull self)
{
    decl_try_err();
    size_t bytesPerPlane = self->bytesPerRow * self->height;

    if (self->pixelFormat == kPixelFormat_RGB_Sprite2) {
        bytesPerPlane += 2*sizeof(uint16_t);    // leading sprite control words
        bytesPerPlane += 2*sizeof(uint16_t);    // trailing sprite control words
    }

    try(kalloc_options(bytesPerPlane, KALLOC_OPTION_UNIFIED, (void**) &self->plane[0]));

    if (self->pixelFormat == kPixelFormat_RGB_Sprite2) {
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

// Allocates a new surface with the given pixel width and height and pixel
// format.
// \param width the width in pixels
// \param height the height in pixels
// \param pixelFormat the pixel format
// \return the surface; NULL on failure
errno_t Surface_Create(int id, int width, int height, PixelFormat pixelFormat, Surface* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Surface* self;
    
    if (width <= 0 || height <= 0) {
        return EINVAL;
    }


    try(kalloc_cleared(sizeof(Surface), (void**) &self));
    
    self->super.type = kGObject_Surface;
    self->super.id = id;
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
    

    *pOutSelf = self;
    return EOK;
    
catch:
    Surface_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

errno_t Surface_CreateNullSprite(Surface* _Nullable * _Nonnull pOutSelf)
{
    Surface* self;
    const errno_t err = Surface_Create(0, 16, 1, kPixelFormat_RGB_Sprite2, &self);

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

// Deallocates the given surface.
// \param pSurface the surface
void Surface_Destroy(Surface* _Nonnull self)
{
    if (self) {
        if ((self->flags & kSurfaceFlag_ClusteredPlanes) != 0) {
            kfree(self->plane[0]);
        }
        else {
            for (int i = 0; i < self->planeCount; i++) {
                kfree(self->plane[i]);
            }
        }
        for (int i = 0; i < self->planeCount; i++) {
            self->plane[i] = NULL;
        }

        kfree(self);
    }
}

errno_t Surface_WritePixels(Surface* _Nonnull self, const uint16_t* _Nonnull planes[])
{
    if (self->pixelFormat != kPixelFormat_RGB_Sprite2) {
        return ENOTSUP;
    }

    const uint16_t* sp0 = planes[0];
    const uint16_t* sp1 = planes[1];
    uint16_t* pp = (uint16_t*)self->plane[0];
    uint16_t* dp = &pp[2];

    for (int i = 0; i < self->height; i++) {
        *dp++ = *sp0++;
        *dp++ = *sp1++;
    }

    return EOK;
}
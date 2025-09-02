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

        default:
            return 1;
    }
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
    self->bytesPerPlane = self->bytesPerRow * height;
    self->planeCount = PixelFormat_GetPlaneCount(pixelFormat);

    // Allocate the planes. Note that we try to cluster the planes whenever possible.
    // This means that we allocate a single contiguous memory range big enough to
    // hold all planes. We only allocate independent planes if we're not able to
    // allocate a big enough contiguous memory region because DMA memory has become
    // too fragmented to pull this off. Individual planes in a clustered planes
    // configuration are aligned on an 8 byte boundary.
    const size_t bytesPerClusteredPlane = __Ceil_PowerOf2(self->bytesPerPlane, 8);
    const size_t clusteredSize = self->planeCount * bytesPerClusteredPlane;

    if (kalloc_options(clusteredSize, KALLOC_OPTION_UNIFIED, (void**) &self->plane[0])) {
        for (int i = 1; i < self->planeCount; i++) {
            self->plane[i] = self->plane[i - 1] + bytesPerClusteredPlane;
        }
        self->bytesPerPlane = bytesPerClusteredPlane;
        self->flags |= kSurfaceFlag_ClusteredPlanes;
    }
    else {
        for (int i = 0; i < self->planeCount; i++) {
            try(kalloc_options(self->bytesPerPlane, KALLOC_OPTION_UNIFIED, (void**) &self->plane[i]));
        }
    }
    
    *pOutSelf = self;
    return EOK;
    
catch:
    Surface_Destroy(self);
    *pOutSelf = NULL;
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

// Maps the surface pixels for access. 'mode' specifies whether the pixels
// will be read, written or both.
// \param self the screen
// \param mode the mapping mode
// \return EOK if the screen pixels could be locked; EBUSY otherwise
errno_t Surface_Map(Surface* _Nonnull self, MapPixels mode, SurfaceMapping* _Nonnull pOutMapping)
{
    if ((self->flags & kSurfaceFlag_IsMapped) == kSurfaceFlag_IsMapped) {
        return EBUSY;
    }

    const size_t planeCount = self->planeCount;

    for (size_t i = 0; i < planeCount; i++) {
        pOutMapping->plane[i] = self->plane[i];
        pOutMapping->bytesPerRow[i] = self->bytesPerRow;
    }
    pOutMapping->planeCount = planeCount;

    self->flags |= kSurfaceFlag_IsMapped;
    return EOK;
}

errno_t Surface_Unmap(Surface* _Nonnull self)
{
    if ((self->flags & kSurfaceFlag_IsMapped) == 0) {
        return EPERM;
    }

    self->flags &= ~kSurfaceFlag_IsMapped;
    return EOK;
}

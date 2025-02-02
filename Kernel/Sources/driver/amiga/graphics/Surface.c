//
//  Surface.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Surface.h"
#include <klib/Kalloc.h>


// Allocates a new surface with the given pixel width and height and pixel
// format.
// \param width the width in pixels
// \param height the height in pixels
// \param pixelFormat the pixel format
// \return the surface; NULL on failure
errno_t Surface_Create(int width, int height, PixelFormat pixelFormat, Surface* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Surface* self;
    
    if (width <= 0 || height <= 0) {
        return EINVAL;
    }

    try(kalloc_cleared(sizeof(Surface), (void**) &self));
    
    self->pixelFormat = pixelFormat;
    self->clutEntryCount = (int16_t)PixelFormat_GetCLUTEntryCount(pixelFormat);

    if (self->clutEntryCount > 0) {
        try(kalloc_cleared(sizeof(CLUTEntry) * self->clutEntryCount, (void**)&self->clut));
    }

    self->width = width;
    self->height = height;
    self->bytesPerRow = ((width + 15) >> 4) << 1;       // Must be a multiple of at least words (16bits)
    self->bytesPerPlane = self->bytesPerRow * height;
    self->planeCount = (int8_t)PixelFormat_GetPlaneCount(pixelFormat);

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
void Surface_Destroy(Surface* _Nullable self)
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
        
        if (self->clut) {
            kfree(self->clut);
            self->clut = NULL;
        }
        
        kfree(self);
    }
}

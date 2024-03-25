//
//  Surface.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Surface.h"


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
    
    if (width < 0 || height < 0) {
        return EINVAL;
    }

    try(kalloc_cleared(sizeof(Surface), (void**) &self));
    
    self->pixelFormat = pixelFormat;

    if (width > 0 && height > 0) {
        self->width = width;
        self->height = height;
        self->bytesPerRow = ((size_t)width + 7) >> 3;
        self->bytesPerPlane = self->bytesPerRow * (size_t)height;
        self->planeCount = (int8_t)PixelFormat_GetPlaneCount(pixelFormat);

        // Allocate the planes. Note that we try to cluster the planes whenever possible.
        // This means that we allocate a single contiguous memory range big enough to
        // hold all planes. We only allocate independent planes if we're not able to
        // allocate a big enough contiguous memory region because DMA memory has become
        // too fragmented to pull this off. Individual planes in a clustered planes
        // configuration are aligned on an 8 byte boundary.
        const size_t bytesPerClusteredPlane = __Ceil_PowerOf2(self->bytesPerPlane, 8);
        const size_t clusteredSize = self->planeCount * bytesPerClusteredPlane;

        if (kalloc_options(clusteredSize, KALLOC_OPTION_UNIFIED, (void**) &self->planes[0])) {
            for (int i = 1; i < self->planeCount; i++) {
                self->planes[i] = self->planes[i - 1] + bytesPerClusteredPlane;
            }
            self->bytesPerPlane = bytesPerClusteredPlane;
            self->flags |= kSurfaceFlag_ClusteredPlanes;
        }
        else {
            for (int i = 0; i < self->planeCount; i++) {
                try(kalloc_options(self->bytesPerPlane, KALLOC_OPTION_UNIFIED, (void**) &self->planes[i]));
            }
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
            kfree(self->planes[0]);
        }
        else {
            for (int i = 0; i < self->planeCount; i++) {
                kfree(self->planes[i]);
            }
        }
        for (int i = 0; i < self->planeCount; i++) {
            self->planes[i] = NULL;
        }
        
        kfree(self);
    }
}

// Locks the surface pixels for access. 'access' specifies whether the pixels
// will be read, written or both.
// \param pSurface the surface
// \param access the access mode
// \return EOK if the surface pixels could be locked; EBUSY otherwise
errno_t Surface_LockPixels(Surface* _Nonnull self, SurfaceAccess access)
{
    if ((self->flags & kSurfaceFlag_Locked) != 0) {
        return EBUSY;
    }
    
    self->flags |= kSurfaceFlag_Locked;
    return EOK;
}

void Surface_UnlockPixels(Surface* _Nonnull self)
{
    assert((self->flags & kSurfaceFlag_Locked) != 0);
    self->flags &= ~kSurfaceFlag_Locked;
}

// Clears all pixels in the surface. Clearing means that all pixels are set to
// color black/index 0.
void Surface_Clear(Surface* _Nonnull self)
{
    // Take advantage of clustered planar configurations by issuing a single
    // memset() across all planes.
    if ((self->flags & kSurfaceFlag_ClusteredPlanes) != 0) {
        memset(self->planes[0], 0, self->planeCount * self->bytesPerPlane);
    }
    else {
        for (int i = 0; i < self->planeCount; i++) {
            memset(self->planes[i], 0, self->bytesPerPlane);
        }
    }
}

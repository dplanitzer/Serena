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
errno_t Surface_Create(int width, int height, PixelFormat pixelFormat, Surface* _Nullable * _Nonnull pOutSurface)
{
    decl_try_err();
    Surface* pSurface;
    
    if (width < 0 || height < 0) {
        return EINVAL;
    }

    try(kalloc_cleared(sizeof(Surface), (void**) &pSurface));
    
    pSurface->width = width;
    pSurface->height = height;
    pSurface->bytesPerRow = ((size_t)width + 7) >> 3;
    pSurface->planeCount = (int8_t)PixelFormat_GetPlaneCount(pixelFormat);
    pSurface->pixelFormat = pixelFormat;
    pSurface->flags = 0;
    
    
    // Allocate the planes
    const size_t bytesPerPlane = pSurface->bytesPerRow * (size_t)pSurface->height;
    
    for (int i = 0; i < pSurface->planeCount; i++) {
        try(kalloc_options(bytesPerPlane, KALLOC_OPTION_UNIFIED, (void**) &pSurface->planes[i]));
    }
    
    *pOutSurface = pSurface;
    return EOK;
    
catch:
    Surface_Destroy(pSurface);
    *pOutSurface = NULL;
    return err;
}

// Deallocates the given surface.
// \param pSurface the surface
void Surface_Destroy(Surface* _Nullable pSurface)
{
    if (pSurface) {
        for (int i = 0; i < pSurface->planeCount; i++) {
            kfree(pSurface->planes[i]);
            pSurface->planes[i] = NULL;
        }
        
        kfree(pSurface);
    }
}

// Locks the surface pixels for access. 'access' specifies whether the pixels
// will be read, written or both.
// \param pSurface the surface
// \param access the access mode
// \return EOK if the surface pixels could be locked; EBUSY otherwise
errno_t Surface_LockPixels(Surface* _Nonnull pSurface, SurfaceAccess access)
{
    if ((pSurface->flags & SURFACE_FLAG_LOCKED) != 0) {
        return EBUSY;
    }
    
    pSurface->flags |= SURFACE_FLAG_LOCKED;
    return EOK;
}

void Surface_UnlockPixels(Surface* _Nonnull pSurface)
{
    assert((pSurface->flags & SURFACE_FLAG_LOCKED) != 0);
    pSurface->flags &= ~SURFACE_FLAG_LOCKED;
}

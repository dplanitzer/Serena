//
//  Surface.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Surface.h"


// Returns how many planes we need to allocate for the given pixel format.
static Int PixelFormat_GetPlaneCount(PixelFormat format)
{
    switch (format) {
        case kPixelFormat_RGB_Indexed1:     return 1;
        case kPixelFormat_RGB_Indexed2:     return 2;
        case kPixelFormat_RGB_Indexed3:     return 3;
        case kPixelFormat_RGB_Indexed4:     return 4;
        case kPixelFormat_RGB_Indexed5:     return 5;
        default:    abort();
    }
}



// Allocates a new surface with the given pixel width and height and pixel
// format.
// \param width the width in pixels
// \param height the height in pixels
// \param pixelFormat the pixel format
// \return the surface; NULL on failure
ErrorCode Surface_Create(Int width, Int height, PixelFormat pixelFormat, Surface* _Nullable * _Nonnull pOutSurface)
{
    decl_try_err();
    Surface* pSurface;
    
    try(kalloc_cleared(sizeof(Surface), (void**) &pSurface));
    
    pSurface->width = width;
    pSurface->height = height;
    pSurface->bytesPerRow = (width + 7) >> 3;
    pSurface->planeCount = PixelFormat_GetPlaneCount(pixelFormat);
    pSurface->pixelFormat = pixelFormat;
    pSurface->flags = 0;
    
    
    // Allocate the planes
    const Int bytesPerPlane = pSurface->bytesPerRow * pSurface->height;
    
    for (Int i = 0; i < pSurface->planeCount; i++) {
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
        for (Int i = 0; i < pSurface->planeCount; i++) {
            kfree(pSurface->planes[i]);
            pSurface->planes[i] = NULL;
        }
        
        kfree(pSurface);
    }
}

Size Surface_GetPixelSize(Surface* _Nonnull pSurface)
{
    return Size_Make(pSurface->width, pSurface->height);
}

// Locks the surface pixels for access. 'access' specifies whether the pixels
// will be read, written or both.
// \param pSurface the surface
// \param access the access mode
// \return EOK if the surface pixels could be locked; EBUSY otherwise
ErrorCode Surface_LockPixels(Surface* _Nonnull pSurface, SurfaceAccess access)
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

//
//  Surface.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Surface.h"
#include "Heap.h"


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
Surface* _Nullable Surface_Create(Int width, Int height, PixelFormat pixelFormat)
{
    Surface* pSurface = (Surface*) kalloc_cleared(sizeof(Surface));
    FailNULL(pSurface);
    
    pSurface->width = width;
    pSurface->height = height;
    pSurface->bytesPerRow = (width + 7) >> 3;
    pSurface->planeCount = PixelFormat_GetPlaneCount(pixelFormat);
    pSurface->pixelFormat = pixelFormat;
    pSurface->flags = 0;
    
    
    // Allocate the planes
    const Int bytesPerPlane = pSurface->bytesPerRow * pSurface->height;
    
    for (Int i = 0; i < pSurface->planeCount; i++) {
        pSurface->planes[i] = kalloc_options(bytesPerPlane, HEAP_ALLOC_OPTION_CPU|HEAP_ALLOC_OPTION_CHIPSET);
        if (pSurface->planes[i] == NULL) goto failed;
    }
    
    return pSurface;
    
failed:
    Surface_Destroy(pSurface);
    return NULL;
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
        
        kfree((Byte*)pSurface);
    }
}

Size Surface_GetPixelSize(Surface* _Nonnull pSurface)
{
    return Size_Make(pSurface->width, pSurface->height);
}

// Locks the surface pixels for access. 'access' specifies whether the pixels
// will be read, written or both.
// \param pSurface the surface
// \param access the acess mode
// \return true if the surface pixels could be locked; false otherwise
Bool Surface_LockPixels(Surface* _Nonnull pSurface, SurfaceAccess access)
{
    assert((pSurface->flags & SURFACE_FLAG_LOCKED) == 0);
    
    pSurface->flags |= SURFACE_FLAG_LOCKED;
    return true;
}

void Surface_UnlockPixels(Surface* _Nonnull pSurface)
{
    assert((pSurface->flags & SURFACE_FLAG_LOCKED) != 0);
    pSurface->flags &= ~SURFACE_FLAG_LOCKED;
}

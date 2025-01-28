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
    self->clutEntryCount = (int16_t)PixelFormat_GetCLUTEntryCount(pixelFormat);

    if (self->clutEntryCount > 0) {
        try(kalloc_cleared(sizeof(CLUTEntry) * self->clutEntryCount, (void**)&self->clut));
    }

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

// Clears all pixels in the surface. Clearing means that all pixels are set to
// color black/index 0.
void Surface_Clear(Surface* _Nonnull self)
{
    // Take advantage of clustered planar configurations by issuing a single
    // memset() across all planes.
    if ((self->flags & kSurfaceFlag_ClusteredPlanes) != 0) {
        memset(self->plane[0], 0, self->planeCount * self->bytesPerPlane);
    }
    else {
        for (int i = 0; i < self->planeCount; i++) {
            memset(self->plane[i], 0, self->bytesPerPlane);
        }
    }
}

// Left edge is inclusive
static uint8_t left_edge_mask(int x)
{
    switch (x & 7) {
        case 0:  return 0xff;
        case 1:  return 0x7f;
        case 2:  return 0x3f;
        case 3:  return 0x1f;
        case 4:  return 0x0f;
        case 5:  return 0x07;
        case 6:  return 0x03;
        default: return 0x01;
    }
}

// Right edge is exclusive
static uint8_t right_edge_mask(int x)
{
    switch (x & 7) {
        case 0:  return 0x00;
        case 1:  return 0x80;
        case 2:  return 0xc0;
        case 3:  return 0xe0;
        case 4:  return 0xf0;
        case 5:  return 0xf8;
        case 6:  return 0xfc;
        default: return 0xfe;
    }
}

// 0 < (x1 - x0) < 8 and both coordinates must be inside the same byte
static uint8_t bit_range_mask(int x0, int x1)
{
    const int s = 7 - (x0 & 7);
    const int e = 7 - (x1 & 7);
    uint8_t b = 0;

    for (int i = e - 1; i >= s; i--) {
        b |= (1 << i);
    }

    return b;
}

// Fills a bitmap with 0s or 1s. Expects that the passed in rectangle is already
// clipped to the bitmap bounds and that the empty case has been handled.
static void bitmap_fill_rect(uint8_t* pBitmap, int l, int t, int r, int b, size_t bytesPerRow, int bOne)
{
    const size_t lByteIdx = (size_t)l >> 3;
    const size_t rByteIdx = (size_t)r >> 3;

    if (lByteIdx == rByteIdx) {
        const uint8_t bitMask = bit_range_mask(l, r);
        const uint8_t invBitMask = ~bitMask; 
        uint8_t* p = pBitmap + t * bytesPerRow + lByteIdx;
        size_t y = t;

        while (y < b) {
            *p = (bOne) ? *p | bitMask : *p & invBitMask;
            p += bytesPerRow;
            y++;
        }
        return;
    }

    const uint8_t lEdgeMask = left_edge_mask(l);
    const uint8_t rEdgeMask = right_edge_mask(r);
    const uint8_t lInvEdgeMask = ~lEdgeMask;
    const uint8_t rInvEdgeMask = ~rEdgeMask;
    const uint8_t mFillByte = (bOne) ? 0xff : 0x00;
    uint8_t* lp = pBitmap + t * bytesPerRow + lByteIdx;
    uint8_t* rp = pBitmap + t * bytesPerRow + rByteIdx;
    size_t y = t;

    while (y < b) {
        *lp = (bOne) ? *lp | lEdgeMask : *lp & lInvEdgeMask;

        uint8_t* mp = lp + 1;
        __ptrdiff_t mbc = rp - mp;
        if (mbc < 8) {
            while (mp < rp) {
                *mp++ = mFillByte;
            }
        }
        else {
            memset(mp, mFillByte, mbc);
        }

        // The right edge mask is exclusive. If 'rEdgeMask' == 0 then we don't
        // want to read or write the 'rp' byte because it could be outside the
        // bounds of the bitmap (rp = lp + 1 in this case).
        if (rEdgeMask) {
            *rp = (bOne) ? *rp | rEdgeMask : *rp & rInvEdgeMask;
        }

        lp += bytesPerRow;
        rp += bytesPerRow;
        y++;
    }
}

void Surface_FillRect(Surface* _Nonnull self, int left, int top, int right, int bottom, Color clr)
{
    const int l = __max(0, left);
    const int t = __max(0, top);
    const int r = __min(self->width, right);
    const int b = __min(self->height, bottom);

    if ((r - l) <= 0 || (b - t) <= 0) {
        return;
    }

    switch (self->pixelFormat) {
        case kPixelFormat_RGB_Indexed1:
        case kPixelFormat_RGB_Indexed2:
        case kPixelFormat_RGB_Indexed3:
        case kPixelFormat_RGB_Indexed4:
        case kPixelFormat_RGB_Indexed5:
            for (int i = 0; i < self->planeCount; i++) {
                bitmap_fill_rect(self->plane[i], l, t, r, b, self->bytesPerRow, clr.u.index & (1 << i));
            }
            break;

        default:
            abort();
    }
}

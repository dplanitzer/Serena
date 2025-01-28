//
//  Surface.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Surface_h
#define Surface_h

#include <klib/klib.h>
#include "Color.h"
#include "PixelFormat.h"


typedef struct CLUTEntry {
    uint8_t     r;
    uint8_t     g;
    uint8_t     b;
    uint8_t     flags;
} CLUTEntry;


#define MAX_PLANE_COUNT  6

enum {
    kSurfaceFlag_ClusteredPlanes = 0x01,    // Surface is planar and all planes share a single kalloc() memory block. Ptr of this memory block is in planes[0]
};

typedef struct Surface {
    uint8_t* _Nullable      plane[MAX_PLANE_COUNT];
    CLUTEntry* _Nullable    clut;
    int                     width;
    int                     height;
    size_t                  bytesPerRow;
    size_t                  bytesPerPlane;
    int16_t                 clutEntryCount;
    int8_t                  planeCount;
    int8_t                  pixelFormat;
    uint8_t                 flags;
} Surface;


extern errno_t Surface_Create(int width, int height, PixelFormat pixelFormat, Surface* _Nullable * _Nonnull pOutSelf);
extern void Surface_Destroy(Surface* _Nullable self);

// Returns the pixel width of the surface.
#define Surface_GetWidth(__self) \
((__self)->width)

// Returns the pixel height of the surface.
#define Surface_GetHeight(__self) \
((__self)->height)

// Returns the bytes-per-row value
#define Surface_GetBytesPerRow(__self) \
((__self)->bytesPerRow)

// Returns the pixel format
#define Surface_GetPixelFormat(__self) \
((__self)->pixelFormat)

// Returns the number of entries in the color lookup table that is associated
// with this surface; 0 is returned if the pixel format is a direct format.
#define Surface_GetCLUTEntryCount(__self) \
((__self)->clutEntryCount)

// Returns a reference to the CLUT entry at index 'idx'
#define Surface_GetCLUTEntry(__self, __idx) \
(&(__self)->clut[__idx])

// Clears all pixels in the surface. Clearing means that all pixels are set to
// color black/index 0.
extern void Surface_Clear(Surface* _Nonnull self);

extern void Surface_FillRect(Surface* _Nonnull self, int left, int top, int right, int bottom, Color clr);

#endif /* Surface_h */

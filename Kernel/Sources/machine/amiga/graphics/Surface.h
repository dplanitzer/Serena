//
//  Surface.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Surface_h
#define Surface_h

#include "GObject.h"
#include <kern/errno.h>
#include <kpi/fb.h>


#define MAX_PLANE_COUNT  6

enum {
    kSurfaceFlag_ClusteredPlanes = 0x01,    // Surface is planar and all planes share a single kalloc() memory block. Ptr of this memory block is in planes[0]
    kSurfaceFlag_IsMapped = 0x02,
};

typedef struct Surface {
    GObject             super;
    uint8_t* _Nullable  plane[MAX_PLANE_COUNT];
    int                 width;
    int                 height;
    size_t              bytesPerRow;
    int8_t              planeCount;
    int8_t              pixelFormat;
    uint8_t             flags;
} Surface;


extern errno_t Surface_Create(int id, int width, int height, PixelFormat pixelFormat, Surface* _Nullable * _Nonnull pOutSelf);

// Create a surface that represents a null sprite.
extern errno_t Surface_CreateNullSprite(Surface* _Nullable * _Nonnull pOutSelf);

extern void Surface_Destroy(Surface* _Nullable self);


// Returns the pixel width of the surface.
#define Surface_GetWidth(__self) \
((__self)->width)

// Returns the pixel height of the surface.
#define Surface_GetHeight(__self) \
((__self)->height)

// Returns the number of planes in the surface.
#define Surface_GetPlaneCount(__self) \
((__self)->planeCount)

// Returns the bytes-per-row value
#define Surface_GetBytesPerRow(__self) \
((__self)->bytesPerRow)

// Returns the pixel format
#define Surface_GetPixelFormat(__self) \
((__self)->pixelFormat)

// Returns the n-th plane of the surface.
#define Surface_GetPlane(__self, __idx) \
((__self)->plane[__idx])


extern errno_t Surface_WritePixels(Surface* _Nonnull self, const uint16_t* _Nonnull planes[]);
extern errno_t Surface_ClearPixels(Surface* _Nonnull self);

#endif /* Surface_h */

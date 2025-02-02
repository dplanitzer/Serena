//
//  Surface.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Surface_h
#define Surface_h

#include <klib/Error.h>
#include <klib/Types.h>
#include "Color.h"
#include "PixelFormat.h"


// Specifies what you want to do with the pixels when you call LockPixels()
typedef enum MapPixels {
    kMapPixels_Read,
    kMapPixels_ReadWrite
} MapPixels;

typedef struct SurfaceMapping {
    void* _Nonnull  plane[8];
    size_t          bytesPerRow[8];
    size_t _Nonnull planeCount;
} SurfaceMapping;


#define MAX_PLANE_COUNT  6

enum {
    kSurfaceFlag_ClusteredPlanes = 0x01,    // Surface is planar and all planes share a single kalloc() memory block. Ptr of this memory block is in planes[0]
    kSurfaceFlag_IsMapped = 0x02,
};

typedef struct Surface {
    uint8_t* _Nullable  plane[MAX_PLANE_COUNT];
    int                 width;
    int                 height;
    int                 bytesPerRow;
    int                 bytesPerPlane;
    int8_t              planeCount;
    int8_t              pixelFormat;
    uint8_t             flags;
    int                 retainCount;
} Surface;


extern errno_t Surface_Create(int width, int height, PixelFormat pixelFormat, Surface* _Nullable * _Nonnull pOutSelf);

#define Surface_Retain(__self) \
((__self)->retainCount++)

extern void Surface_Release(Surface* _Nullable self);

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

extern errno_t Surface_Map(Surface* _Nonnull self, MapPixels mode, SurfaceMapping* _Nonnull pOutInfo);
extern errno_t Surface_Unmap(Surface* _Nonnull self);

#endif /* Surface_h */

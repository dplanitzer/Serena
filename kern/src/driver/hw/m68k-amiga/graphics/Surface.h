//
//  Surface.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Surface_h
#define Surface_h

#include <ext/try.h>
#include <ext/queue.h>
#include <kpi/framebuffer.h>


enum {
    kSurfaceFlag_ClusteredPlanes = 0x01,    // Surface is planar and all planes share a single kalloc() memory block. Ptr of this memory block is in planes[0]
    kSurfaceFlag_IsMapped = 0x02,
    kSurfaceFlag_IsRegistered = 0x04,
};

typedef struct Surface {
    deque_node_t        chain;
    int                 id;
    int                 refCount;
    uint8_t* _Nullable  plane[8];
    int                 width;
    int                 height;
    size_t              bytesPerRow;
    vio_pixfmt_t            pixelFormat;
    int8_t              planeCount;
    uint8_t             flags;
} Surface;


extern errno_t Surface_Create(int width, int height, vio_pixfmt_t pixelFormat, Surface* _Nullable * _Nonnull pOutSelf);

// Create a surface that represents a null sprite.
extern errno_t Surface_CreateNullSprite(Surface* _Nullable * _Nonnull pOutSelf);

#define Surface_AddRef(__self) \
(((Surface*)(__self))->refCount++)

extern void Surface_DelRef(Surface* _Nullable self);

extern Surface* _Nullable Surface_GetForId(int id);


#define Surface_GetId(__self) \
(((Surface*)(__self))->id)

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

// Returns true if the surface is currently mapped
#define Surface_IsMapped(__self) \
(((__self)->flags & kSurfaceFlag_IsMapped) == kSurfaceFlag_IsMapped)

extern errno_t Surface_WritePixels(Surface* _Nonnull self, const void* _Nonnull planes[], size_t bytesPerRow, vio_pixfmt_t format);
extern errno_t Surface_ClearPixels(Surface* _Nonnull self);

#endif /* Surface_h */

//
//  Surface.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Surface_h
#define Surface_h

#include <kern/errno.h>
#include <kern/types.h>
#include <klib/List.h>
#include <sys/fb.h>


#define MAX_PLANE_COUNT  6

enum {
    kSurfaceFlag_ClusteredPlanes = 0x01,    // Surface is planar and all planes share a single kalloc() memory block. Ptr of this memory block is in planes[0]
    kSurfaceFlag_IsMapped = 0x02,
};

typedef struct Surface {
    ListNode            chain;
    uint8_t* _Nullable  plane[MAX_PLANE_COUNT];
    int                 width;
    int                 height;
    int                 bytesPerRow;
    int                 bytesPerPlane;
    int8_t              planeCount;
    int8_t              pixelFormat;
    uint8_t             flags;
    int                 useCount;
    int                 id;
} Surface;


extern errno_t Surface_Create(int id, int width, int height, PixelFormat pixelFormat, Surface* _Nullable * _Nonnull pOutSelf);
extern void Surface_Destroy(Surface* _Nullable self);

#define Surface_BeginUse(__self) \
((__self)->useCount++)

#define Surface_EndUse(__self) \
((__self)->useCount--)

#define Surface_IsUsed(__self) \
((__self)->useCount > 0)


#define Surface_GetId(__self) \
((__self)->id)


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


extern errno_t Surface_Map(Surface* _Nonnull self, MapPixels mode, SurfaceMapping* _Nonnull pOutMapping);
extern errno_t Surface_Unmap(Surface* _Nonnull self);

#endif /* Surface_h */

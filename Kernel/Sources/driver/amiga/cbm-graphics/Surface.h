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
#include <driver/amiga/cbm-graphics/PixelFormat.h>


// Specifies what you want to do with the pixels when you call LockPixels()
typedef enum SurfaceAccess {
    kSurfaceAccess_Read,
    kSurfaceAccess_Write
} SurfaceAccess;


#define MAX_PLANE_COUNT  6

enum {
    kSurfaceFlag_Locked = 0x01,
    kSurfaceFlag_ClusteredPlanes = 0x02,    // Surface is planar and all planes share a single kalloc() memory block. Ptr of this memory block is in planes[0]
};

typedef struct Surface {
    uint8_t* _Nullable  planes[MAX_PLANE_COUNT];
    int                 width;
    int                 height;
    size_t              bytesPerRow;
    int8_t              planeCount;
    int8_t              pixelFormat;
    uint8_t             flags;
} Surface;


extern errno_t Surface_Create(int width, int height, PixelFormat pixelFormat, Surface* _Nullable * _Nonnull pOutSurface);
extern void Surface_Destroy(Surface* _Nullable pSurface);

// Returns the pixel width of the surface.
#define Surface_GetWidth(__self) \
(__self)->width

// Returns the pixel height of the surface.
#define Surface_GetHeight(__self) \
(__self)->height

// Locks the surface for read or read and write access. You must lock a surface
// for read and write access before you can call any of the drawing related
// functions. You must unlock the surface once you are done drawing. 
extern errno_t Surface_LockPixels(Surface* _Nonnull pSurface, SurfaceAccess access);

// Unlocks the surface.
extern void Surface_UnlockPixels(Surface* _Nonnull pSurface);

#endif /* Surface_h */

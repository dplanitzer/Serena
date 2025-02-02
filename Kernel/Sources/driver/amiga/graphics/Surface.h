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

typedef struct MappingInfo {
    void* _Nonnull  plane[8];
    size_t          bytesPerRow[8];
    size_t _Nonnull planeCount;
} MappingInfo;


typedef struct CLUTEntry {
    uint8_t     r;
    uint8_t     g;
    uint8_t     b;
    uint8_t     flags;
} CLUTEntry;


#define MAX_PLANE_COUNT  6

enum {
    kSurfaceFlag_ClusteredPlanes = 0x01,    // Surface is planar and all planes share a single kalloc() memory block. Ptr of this memory block is in planes[0]
    kSurfaceFlag_IsMapped = 0x02,
};

typedef struct Surface {
    uint8_t* _Nullable      plane[MAX_PLANE_COUNT];
    CLUTEntry* _Nullable    clut;
    int                     width;
    int                     height;
    int                     bytesPerRow;
    int                     bytesPerPlane;
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

extern errno_t Surface_SetCLUTEntry(Surface* _Nonnull self, size_t idx, RGBColor32 color);
extern errno_t Surface_SetCLUTEntries(Surface* _Nonnull self, size_t idx, size_t count, const RGBColor32* _Nonnull entries);

extern errno_t Surface_Map(Surface* _Nonnull self, MapPixels mode, MappingInfo* _Nonnull pOutInfo);
extern errno_t Surface_Unmap(Surface* _Nonnull self);

#endif /* Surface_h */

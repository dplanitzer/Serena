//
//  kpi/fb.h
//  libc
//
//  Created by Dietmar Planitzer on 2/2/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_FB_H
#define _KPI_FB_H 1

#include <_cmndef.h>
#include <kpi/ioctl.h>
#include <kpi/types.h>

// The pixel formats supported by framebuffers
enum {
    kPixelFormat_RGB_Indexed1,      // planar indexed RGB with 1 plane
    kPixelFormat_RGB_Indexed2,      // planar indexed RGB with 2 planes
    kPixelFormat_RGB_Indexed3,      // planar indexed RGB with 3 planes
    kPixelFormat_RGB_Indexed4,      // planar indexed RGB with 4 planes
    kPixelFormat_RGB_Indexed5,      // planar indexed RGB with 5 planes
};
typedef int PixelFormat;


// Geometry and pixel encoding of a surface
typedef struct SurfaceInfo {
    int         width;
    int         height;
    PixelFormat pixelFormat;
} SurfaceInfo;


// Specifies what you want to do with the pixels when you call map_surface()
enum {
    kMapPixels_Read,
    kMapPixels_ReadWrite
};
typedef int MapPixels;


// Provides access to the pixel data of a surface
typedef struct SurfaceMapping {
    void* _Nonnull  plane[8];
    size_t          bytesPerRow[8];
    size_t _Nonnull planeCount;
} SurfaceMapping;


// CLUT information
typedef struct CLUTInfo {
    size_t  entryCount;
} CLUTInfo;


//
// Colors
//

// 32bit opaque RGB color
typedef unsigned int RGBColor32;


// Returns a packed 32bit RGB color value
#define RGBColor32_Make(__r, __g, __b) \
    ((((__r) & 0xff) << 16) | (((__g) & 0xff) << 8) | ((__b) & 0xff))

// Returns the red component of a RGB32
#define RGBColor32_GetRed(__clr) \
    (((__clr) >> 16) & 0xff)

// Returns the green component of a RGB32
#define RGBColor32_GetGreen(__clr) \
    (((__clr) >> 8) & 0xff)

// Returns the blue component of a RGB32
#define RGBColor32_GetBlue(__clr) \
    ((__clr) & 0xff)


//
// Surfaces
//

// Creates a surface of size 'width' x 'height' pixels and with a pixel encoding
// 'encoding' and returns the unique id of the surface in 'pOutId'. Note that the
// surface width and height have to be > 1. The surface may be used to create a
// screen and it may be directly mapped into the address space of the owning
// process or manipulated with the Blitter.
// create_surface(int width, int height, PixelFormat pixelFormat, int* _Nonnull pOutId)
#define kFBCommand_CreateSurface    IOResourceCommand(kDriverCommand_SubclassBase + 0)

// Destroys the surface with id 'id'. Returns EBUSY if the surface is currently
// mapped or is attached to a screen.
// destroy_surface(int id)
#define kFBCommand_DestroySurface   IOResourceCommand(kDriverCommand_SubclassBase + 1)

// Returns information about the surface 'id'.
// get_surface_info(int id, SurfaceInfo* _Nonnull pOutInfo)
#define kFBCommand_GetSurfaceInfo   IOResourceCommand(kDriverCommand_SubclassBase + 2)

// Maps the backing store of the surface 'id' into the address space of the
// calling process to allow direct access to the pixel data. 'mode' specifies
// whether the pixel data should be mapped for reading only or reading and
// writing. Returns with 'pOutMapping' filled in.
// map_surface(int id, MapPixels mode, SurfaceMapping* _Nonnull pOutMapping)
#define kFBCommand_MapSurface       IOResourceCommand(kDriverCommand_SubclassBase + 3)

// Unmaps the backing store of the surface 'id' and revokes access to the pixels.
// unmap_surface(int id)
#define kFBCommand_UnmapSurface     IOResourceCommand(kDriverCommand_SubclassBase + 4)


//
// CLUTs
//

// Creates a new CLUT with 'entryCount' color entries.
// create_clut(size_t entryCount, int* _Nonnull pOutId)
#define kFBCommand_CreateCLUT       IOResourceCommand(kDriverCommand_SubclassBase + 5)

// Destroys the CLUT with id 'id'. Returns EBUSY if the CLUT is currently in use.
// destroy_clut(int id)
#define kFBCommand_DestroyCLUT      IOResourceCommand(kDriverCommand_SubclassBase + 6)

// Returns information about the CLUT 'id'.
// get_clut_info(int id, CLUTInfo* _Nonnull pOutInfo)
#define kFBCommand_GetCLUTInfo      IOResourceCommand(kDriverCommand_SubclassBase + 7)

// Updates the color entries if the CLUT 'id'. 'count' entries starting at index
// 'idx' are replaced with the color values stored in the array 'entries'.
// set_clut_entries(int id, size_t idx, size_t count, const RGBColor32* _Nonnull entries)
#define kFBCommand_SetCLUTEntries  IOResourceCommand(kDriverCommand_SubclassBase + 8)


// Acquire the sprite with display priority 'priority'. The sprite has a size of
// 'width' x ' height' pixels and a pixel format of 'pixelFormat'. The visual
// priority of the sprite is 'priority'. Note that a driver typically only
// supports a limited number of sprites. The exact limits are platform and
// hardware dependent. Returns ENOTSUP or EBUSY if the requested sprite is not
// available for acquisition. 
// acquire_sprite(int width, int height, PixelFormat pixelFormat, int priority, int* _Nonnull pOutId)
#define kFBCommand_AcquireSprite        IOResourceCommand(kDriverCommand_SubclassBase + 9)

// Relinquishes a previously acquired sprite and makes it available again for
// acquisition.
// relinquish_sprite(int spriteId)
#define kFBCommand_RelinquishSprite     IOResourceCommand(kDriverCommand_SubclassBase + 10)

// Replaces the pixels of a sprite with the given pixels. The given pixel map
// must have the same size as the sprite. 
// set_sprite_pixels(int spriteId, const uint16_t* _Nonnull planes[2])
#define kFBCommand_SetSpritePixels      IOResourceCommand(kDriverCommand_SubclassBase + 11)

// Sets the position of a sprite. Note that sprites are only visible inside the
// screen aperture rectangle.
// set_sprite_position(int spriteId, int x, int y)
#define kFBCommand_SetSpritePosition    IOResourceCommand(kDriverCommand_SubclassBase + 12)

// Shows or hides a sprite.
// set_sprite_visible(int spriteId, bool isVisible)
#define kFBCommand_SetSpriteVisible     IOResourceCommand(kDriverCommand_SubclassBase + 13)


//
// Display
//

#define SCREEN_CONFIG_FRAMEBUFFER   1
#define SCREEN_CONFIG_CLUT          2
#define SCREEN_CONFIG_WIDTH         3
#define SCREEN_CONFIG_HEIGHT        4
#define SCREEN_CONFIG_PIXELFORMAT   5
#define SCREEN_CONFIG_END           0

// Configures the screen based on the given screen configuration. Pass NULL to
// turn video output off altogether.
// set_screen_config(const int* _Nullable config)
#define kFBCommand_SetScreenConfig  IOResourceCommand(kDriverCommand_SubclassBase + 14)


// Returns a copy of the currently active screen configuration. The configuration
// information is written to the provided buffer 'config' which is able to hold
// 'bufsiz' integer entries (sizeof(int)). EINVAL is returned if 'bufsiz' is 0.
// ERANGE is returned if 'bufsiz' is greater 0 but not big enough to hold all
// configuration information plus the terminated SCREEN_CONFIG_END entry. The
// returned configuration will contain the following configuration keys:
// SCREEN_CONFIG_FRAMEBUFFER
// SCREEN_CONFIG_CLUT (if the pixel format is one of the indirect formats)
// SCREEN_CONFIG_WIDTH
// SCREEN_CONFIG_HEIGHT
// SCREEN_CONFIG_PIXELFORMAT
// SCREEN_CONFIG_END
// int get_screen_config(int* _Nonnull config, size_t bufsiz)
#define kFBCommand_GetScreenConfig  IOResourceCommand(kDriverCommand_SubclassBase + 15)

// Updates the display configuration. Call this function after changing the
// following screen properties:
// - CLUT entries
// int update_display()
#define kFBCommand_UpdateDisplay    IOResourceCommand(kDriverCommand_SubclassBase + 16)

#endif /* _KPI_FB_H */

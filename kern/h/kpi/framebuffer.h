//
//  kpi/framebuffer.h
//  kpi
//
//  Created by Dietmar Planitzer on 2/2/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_FRAMEBUFFER_H
#define _KPI_FRAMEBUFFER_H 1

#include <_cmndef.h>
#include <kpi/ioctl.h>
#include <kpi/types.h>

// Surface pixel formats
#define PIXFMT_RGB_IND_1    1  // planar indexed RGB with 1 plane
#define PIXFMT_RGB_IND_2    2  // planar indexed RGB with 2 planes
#define PIXFMT_RGB_IND_3    3  // planar indexed RGB with 3 planes
#define PIXFMT_RGB_IND_4    4  // planar indexed RGB with 4 planes
#define PIXFMT_RGB_IND_5    5  // planar indexed RGB with 5 planes
#define PIXFMT_RGB_IND_6    6  // planar indexed RGB with 6 planes
#define PIXFMT_RGB_IND_7    7  // planar indexed RGB with 7 planes
#define PIXFMT_RGB_IND_8    8  // planar indexed RGB with 8 planes

#ifdef MACHINE_AMIGA
#define PIXFMT_RGB_HAM_5    9   // planar RGB Hold-And-Modify mode with 5 planes
#define PIXFMT_RGB_HAM_6    10  // planar RGB Hold-And-Modify mode with 6 planes
#define PIXFMT_RGB_EHB_6    11  // planar RGB Extra-Half-Bright mode with 6 planes
#define PIXFMT_RGB_SPRITE_2 12  // interleaved planar indexed RGB with 2 planes
#endif
typedef int pixfmt_t;


// Surface binding targets
    //#define TARGET_FRAMEBUFFER    0x10000
#define TARGET_SPRITE_0 0x20000
#define TARGET_SPRITE_1 0x20001
#define TARGET_SPRITE_2 0x20002
#define TARGET_SPRITE_3 0x20003
#define TARGET_SPRITE_4 0x20004
#define TARGET_SPRITE_5 0x20005
#define TARGET_SPRITE_6 0x20006
#define TARGET_SPRITE_7 0x20007


// Geometry and pixel encoding of a surface
typedef struct surface_info {
    int         width;
    int         height;
    pixfmt_t    pixelFormat;
} surface_info_t;


// Specifies what you want to do with the pixels when you call map_surface()
#define SURFACE_MAP_R   0
#define SURFACE_MAP_RW  1


// Provides access to the pixel data of a surface
typedef struct surface_mapping {
    void* _Nonnull  plane[8];
    size_t          planeCount;
    size_t          bytesPerRow;
} surface_mapping_t;


// CLUT information
typedef struct clut_info {
    size_t  entryCount;
} clut_info_t;


//
// Colors
//

// 32bit opaque RGB color
typedef unsigned int color_rgb32_t;


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


#define kRGBColor32_Black  0xff000000
#define kRGBColor32_White  0xffffffff


//
// Surfaces
//

// Creates a 2d surface of size 'width' x 'height' pixels and with a pixel encoding
// 'encoding' and returns the unique id of the surface in 'pOutId'. Note that the
// surface width and height have to be > 1. The surface may be used to create a
// screen and it may be directly mapped into the address space of the owning
// process or manipulated with the Blitter.
// create_surface(int width, int height, pixfmt_t pixelFormat, int* _Nonnull pOutId)
#define kFBCommand_CreateSurface2d  IOResourceCommand(kDriverCommand_SubclassBase + 0)

// Destroys the surface with id 'id'. Returns EBUSY if the surface is currently
// mapped or is attached to a screen. Automatically unbinds the surface if it is
// attached to a sprite and binds the sprite target to a null surface. Does
// nothing if 'id' is 0.
// destroy_surface(int id)
#define kFBCommand_DestroySurface   IOResourceCommand(kDriverCommand_SubclassBase + 1)

// Binds the surface 'id' to the target 'target'. If the target is already bound
// to a surface then that surface is unbound before the new one is bound.
// Binding a target to a surface with id 0 leaves the target unbound.
// bind_surface(int target, int id)
#define kFBCommand_BindSurface  IOResourceCommand(kDriverCommand_SubclassBase + 2)

// Returns information about the surface 'id'.
// get_surface_info(int id, surface_info_t* _Nonnull pOutInfo)
#define kFBCommand_GetSurfaceInfo   IOResourceCommand(kDriverCommand_SubclassBase + 3)

// Maps the backing store of the surface 'id' into the address space of the
// calling process to allow direct access to the pixel data. 'mode' specifies
// whether the pixel data should be mapped for reading only or reading and
// writing. Returns with 'pOutMapping' filled in.
// map_surface(int id, int mode, surface_mapping_t* _Nonnull pOutMapping)
#define kFBCommand_MapSurface       IOResourceCommand(kDriverCommand_SubclassBase + 4)

// Unmaps the backing store of the surface 'id' and revokes access to the pixels.
// unmap_surface(int id)
#define kFBCommand_UnmapSurface     IOResourceCommand(kDriverCommand_SubclassBase + 5)

// Writes pixels to the surface 'id'. The provided source pixel buffer must be
// of the same width and height as the surface. Returns ENOTSUP if the source
// pixels can not be converted to the surface pixel format.
// write_pixels(int id, const void* _Nonnull planes[], size_t bytesPerRow, pixfmt_t format)
#define kFBCommand_WritePixels      IOResourceCommand(kDriverCommand_SubclassBase + 6)

// Clears all pixels of the surface 'id'. Clearing pixels means that all bits in
// all pixels are set to 0.
// clear_pixels(int id)
#define kFBCommand_ClearPixels      IOResourceCommand(kDriverCommand_SubclassBase + 7)


//
// CLUTs
//

// Creates a new CLUT with 'entryCount' color entries.
// create_clut(size_t entryCount, int* _Nonnull pOutId)
#define kFBCommand_CreateCLUT       IOResourceCommand(kDriverCommand_SubclassBase + 8)

// Destroys the CLUT with id 'id'. Returns EBUSY if the CLUT is currently in use.
// destroy_clut(int id)
#define kFBCommand_DestroyCLUT      IOResourceCommand(kDriverCommand_SubclassBase + 9)

// Returns information about the CLUT 'id'.
// get_clut_info(int id, clut_info_t* _Nonnull pOutInfo)
#define kFBCommand_GetCLUTInfo      IOResourceCommand(kDriverCommand_SubclassBase + 10)

// Updates the color entries if the CLUT 'id'. 'count' entries starting at index
// 'idx' are replaced with the color values stored in the array 'entries'.
// set_clut_entries(int id, size_t idx, size_t count, const color_rgb32_t* _Nonnull entries)
#define kFBCommand_SetCLUTEntries  IOResourceCommand(kDriverCommand_SubclassBase + 11)


//
// Sprites
//

typedef struct sprite_caps {
    int minWidth, maxWidth;
    int minHeight, maxHeight;
    int lowSpriteNum, highSpriteNum;
    int xScale;     // One sprite pixel corresponds to 'xScale' screen pixels along the horizontal axis
    int yScale;
} sprite_caps_t;

// Returns information about the limits of the sprite sub-system based on the
// currently active screen and mouse cursor configuration.
// get_sprite_info(sprite_info_t* _Nonnull info)
#define kFBCommand_GetSpriteCaps        IOResourceCommand(kDriverCommand_SubclassBase + 12)

// Sets the position of a sprite. Note that sprites are only visible inside the
// screen aperture rectangle.
// set_sprite_position(int spriteId, int x, int y)
#define kFBCommand_SetSpritePosition    IOResourceCommand(kDriverCommand_SubclassBase + 13)

// Shows or hides a sprite.
// set_sprite_visible(int spriteId, int isVisible)
#define kFBCommand_SetSpriteVisible     IOResourceCommand(kDriverCommand_SubclassBase + 14)


//
// Screen
//

#define SCREEN_CONF_FRAMEBUFFER   1
#define SCREEN_CONF_CLUT          2
#define SCREEN_CONF_WIDTH         3
#define SCREEN_CONF_HEIGHT        4
#define SCREEN_CONF_PIXELFORMAT   5
#define SCREEN_CONF_END           0

// Configures the screen based on the given screen configuration. Pass NULL to
// turn video output off altogether.
// set_screen_config(const intptr_t* _Nullable config)
#define kFBCommand_SetScreenConfig  IOResourceCommand(kDriverCommand_SubclassBase + 15)


// Returns a copy of the currently active screen configuration. The configuration
// information is written to the provided buffer 'config' which is able to hold
// 'bufsiz' integer entries (sizeof(int)). EINVAL is returned if 'bufsiz' is 0.
// ERANGE is returned if 'bufsiz' is greater 0 but not big enough to hold all
// configuration information plus the terminated SCREEN_CONF_END entry. The
// returned configuration will contain the following configuration keys:
// SCREEN_CONF_FRAMEBUFFER
// SCREEN_CONF_CLUT (if the pixel format is one of the indirect formats)
// SCREEN_CONF_WIDTH
// SCREEN_CONF_HEIGHT
// SCREEN_CONF_PIXELFORMAT
// SCREEN_CONF_END
// int get_screen_config(intptr_t* _Nonnull config, size_t bufsiz)
#define kFBCommand_GetScreenConfig  IOResourceCommand(kDriverCommand_SubclassBase + 16)

// Updates the color entries if the current screen CLUT. 'count' entries starting
// at index 'idx' are replaced with the color values stored in the array 'entries'.
// set_screen_clut_entries(int id, size_t idx, size_t count, const color_rgb32_t* _Nonnull entries)
#define kFBCommand_SetScreenCLUTEntries IOResourceCommand(kDriverCommand_SubclassBase + 17)

#endif /* _KPI_FRAMEBUFFER_H */

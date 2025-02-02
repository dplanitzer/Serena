//
//  Framebuffer.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/2/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_FRAMEBUFFER_H
#define _SYS_FRAMEBUFFER_H 1

#include <System/IOChannel.h>

__CPP_BEGIN

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


typedef struct VideoConfiguration {
    int     width;
    int     height;
    int     fps;
} VideoConfiguration;


//
// Colors
//

// 32bit opaque RGB color
typedef uint32_t RGBColor32;


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
// process or manipulated with the blitter.
// create_surface(int width, int height, PixelFormat pixelFormat, int* _Nonnull pOutId)
#define kFBCommand_CreateSurface    IOResourceCommand(0)

// Destroys the surface with id 'id'. Returns EBUSY if the surface is currently
// mapped or is attached to a screen.
// destroy_surface(int id)
#define kFBCommand_DestroySurface   IOResourceCommand(1)

// Returns information about the surface 'id'.
// get_surface_info(int id, SurfaceInfo* _Nonnull pOutInfo)
#define kFBCommand_GetSurfaceInfo   IOResourceCommand(2)

// Maps the backing store of the surface 'id' into the address space of the
// calling process to allow direct access to the pixel data. 'mode' specifies
// whether the pixel data should be mapped for reading only or reading and
// writing. Returns with 'pOutMapping' filled in.
// map_surface(int id, MapPixels mode, SurfaceMapping* _Nonnull pOutMapping)
#define kFBCommand_MapSurface       IOResourceCommand(3)

// Unmaps the backing store of the surface 'id' and revokes access to the pixels.
// unmap_surface(int id)
#define kFBCommand_UnmapSurface     IOResourceCommand(4)


//
// Screens
//

// Creates a new screen based on the given screen configuration. The screen is
// not visible by default. Make it visible by calling set_current_screen() with
// the id that this function returns.
// create_screen(const VideoConfiguration* _Nonnull cfg, int surfaceId)
#define kFBCommand_CreateScreen     IOResourceCommand(256)

// Destroys the screen with id 'id'. Returns EBUSY if the screen is currently
// being shown on the display.
// destroy_screen(int id)
#define kFBCommand_DestroyScreen    IOResourceCommand(257)


// Updates the CLUT entries of the screen 'id'. 'count' entries starting at index
// 'idx' are replaced with the color values stored in the array 'entries'.
// set_clut_entries(int id, size_t idx, size_t count, const RGBColor32* _Nonnull entries)
#define kFBCommand_SetCLUTEntries   IOResourceCommand(258)


// TBD.
// acquire_sprite(int screenId, const uint16_t* _Nonnull pPlanes[2], int x, int y, int width, int height, int priority, int* _Nonnull pOutSpriteId)
#define kFBCommand_AcquireSprite    IOResourceCommand(259)

// TBD.
// relinquish_sprite(int screenId, int spriteId)
#define kFBCommand_RelinquishSprite IOResourceCommand(260)

// TBD.
// set_sprite_position(int screenId, int spriteId, int x, int y)
#define kFBCommand_SetSpritePosition    IOResourceCommand(261)

// TBD.
// set_sprite_visible(int screenId, int spriteId, bool isVisible)
#define kFBCommand_SetSpriteVisible     IOResourceCommand(262)


//
// Display
//

// Makes the screen 'id' the current screen visible to the user. Call this
// function with a screen id of 0 to turn video output off altogether.
// set_current_screen(int id)
#define kFBCommand_SetCurrentScreen IOResourceCommand(512)

// Returns the unique id of the currently visible screen. 0 is returned if no
// screen is visible and video is turned off.
// int get_current_screen()
#define kFBCommand_GetCurrentScreen IOResourceCommand(513)

// Updates the display configuration. Call this function after changing the
// following screen properties:
// - CLUT entries
// int update_display()
#define kFBCommand_UpdateDisplay    IOResourceCommand(514)

// Shields the mouse cursor. Call this function before drawing into the provided
// rectangular region inside a screen's surface to ensure that the mouse cursor
// image will be saved and restored as needed.
// shield_mouse_cursor(int x, int y, int width, int height)
#define kFBCommand_ShieldMouseCursor    IOResourceCommand(515)

// Unshields the mouse cursor and makes it visible again if it was visible before
// shielding. Call this function after you are done drawing into a screen's
// surface.
// int unshield_mouse_cursor()
#define kFBCommand_UnshieldMouseCursor  IOResourceCommand(516)


__CPP_END

#endif /* _SYS_FRAMEBUFFER_H */

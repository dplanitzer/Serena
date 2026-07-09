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

// Buffer pixel formats
#define VIO_COLOR_INDEX1    1  // planar indexed with 1 bit per pixel
#define VIO_COLOR_INDEX2    2  // planar indexed with 2 bits per pixel
#define VIO_COLOR_INDEX3    3  // planar indexed with 3 bits per pixel
#define VIO_COLOR_INDEX4    4  // planar indexed with 4 bits per pixel
#define VIO_COLOR_INDEX5    5  // planar indexed with 5 bits per pixel
#define VIO_COLOR_INDEX6    6  // planar indexed with 6 bits per pixel
#define VIO_COLOR_INDEX7    7  // planar indexed with 7 bits per pixel
#define VIO_COLOR_INDEX8    8  // planar indexed with 8 bits per pixel

#ifdef MACHINE_AMIGA
#define VIO_RGB_HAM_5    9   // planar RGB Hold-And-Modify mode with 5 planes
#define VIO_RGB_HAM_6    10  // planar RGB Hold-And-Modify mode with 6 planes
#define VIO_RGB_EHB_6    11  // planar RGB Extra-Half-Bright mode with 6 planes
#define VIO_RGB_SPRITE_2 12  // interleaved planar indexed RGB with 2 planes
#endif
typedef int vio_pixfmt_t;


// Pixel buffer binding targets
    //#define VIO_FRAMEBUFFER    0x10000
#define VIO_SPRITE_0 0x20000
#define VIO_SPRITE_1 0x20001
#define VIO_SPRITE_2 0x20002
#define VIO_SPRITE_3 0x20003
#define VIO_SPRITE_4 0x20004
#define VIO_SPRITE_5 0x20005
#define VIO_SPRITE_6 0x20006
#define VIO_SPRITE_7 0x20007


// Geometry and pixel encoding of a pixel buffer
typedef struct vio_buffer_info {
    int             width;
    int             height;
    vio_pixfmt_t    pixelFormat;
} vio_buffer_info_t;


// Specifies what you want to do with the pixels when you call map_buffer()
#define VIO_MAP_RDONLY  0
#define VIO_MAP_RW      1


// Provides access to the pixels of a pixel buffer
typedef struct vio_buffer_data {
    void* _Nonnull  plane[8];
    size_t          planeCount;
    size_t          bytesPerRow;
} vio_buffer_data_t;


// CLUT information
typedef struct vio_clut_info {
    size_t  entryCount;
} vio_clut_info_t;


//
// Colors
//

// 32bit opaque RGB color
typedef unsigned int vio_rgb32_t;


// Returns a packed 32bit RGB color value
#define VIO_RGB32_MAKE(__r, __g, __b) \
    ((((__r) & 0xff) << 16) | (((__g) & 0xff) << 8) | ((__b) & 0xff))

// Returns the red component of a RGB32
#define VIO_RGB32_RED(__clr) \
    (((__clr) >> 16) & 0xff)

// Returns the green component of a RGB32
#define VIO_RGB32_GREEN(__clr) \
    (((__clr) >> 8) & 0xff)

// Returns the blue component of a RGB32
#define VIO_RGB32_BLUE(__clr) \
    ((__clr) & 0xff)


#define VIO_RGB32_BLACK  0xff000000
#define VIO_RGB32_WHITE  0xffffffff


//
// Pixel buffers
//

// Creates a 2d pixel buffer of size 'width' x 'height' pixels and with a pixel
// encoding 'encoding' and returns the unique id of the buffer in 'pOutId'. Note
// that the buffer width and height have to be > 1. The buffer may be used to
// create a screen and it may be directly mapped into the address space of the
// owning process or manipulated with the Blitter.
// create_buffer(int width, int height, vio_pixfmt_t pixelFormat, int* _Nonnull pOutId)
#define VIO_CMD_CREATE_BUFFER \
IOCMD_MAKE(IOPROTO_FB, 1, _IOCMD_ACC_WR, 0)

// Destroys the buffer with id 'id'. Returns EBUSY if the buffer is currently
// mapped or is attached to a screen. Automatically unbinds the buffer if it is
// attached to a sprite and binds the sprite target to a null buffer. Does
// nothing if 'id' is 0.
// destroy_buffer(int id)
#define VIO_CMD_DESTROY_BUFFER \
IOCMD_MAKE(IOPROTO_FB, 2, _IOCMD_ACC_WR, 0)

// Binds the buffer 'id' to the target 'target'. If the target is already bound
// to a buffer then that buffer is unbound before the new one is bound.
// Binding a target to a buffer with id 0 leaves the target unbound.
// bind_buffer(int target, int id)
#define VIO_CMD_BIND_BUFFER \
IOCMD_MAKE(IOPROTO_FB, 3, _IOCMD_ACC_WR, 0)

// Returns information about the buffer 'id'.
// get_vio_buffer_info(int id, vio_buffer_info_t* _Nonnull pOutInfo)
#define VIO_CMD_BUFFER_INFO \
IOCMD_MAKE(IOPROTO_FB, 4, _IOCMD_ACC_RD, 0)

// Maps the backing store of the buffer 'id' into the address space of the
// calling process to allow direct access to the pixel data. 'mode' specifies
// whether the pixel data should be mapped for reading only or reading and
// writing. Returns with 'pOutMapping' filled in.
// map_buffer(int id, int mode, vio_buffer_data_t* _Nonnull pOutMapping)
#define VIO_CMD_MAP_BUFFER \
IOCMD_MAKE(IOPROTO_FB, 5, _IOCMD_ACC_RDWR, 0)

// Unmaps the backing store of the buffer 'id' and revokes access to the pixels.
// unmap_buffer(int id)
#define VIO_CMD_UNMAP_BUFFER \
IOCMD_MAKE(IOPROTO_FB, 6, _IOCMD_ACC_RDWR, 0)

// Writes pixels to the buffer 'id'. The provided source pixel buffer must be
// of the same width and height as the buffer. Returns ENOTSUP if the source
// pixels can not be converted to the buffer pixel format.
// write_pixels(int id, const void* _Nonnull planes[], size_t bytesPerRow, vio_pixfmt_t format)
#define VIO_CMD_WRITE_PIXELS \
IOCMD_MAKE(IOPROTO_FB, 7, _IOCMD_ACC_WR, 0)

// Clears all pixels of the buffer 'id'. Clearing pixels means that all bits in
// all pixels are set to 0.
// clear_pixels(int id)
#define VIO_CMD_CLEAR_PIXELS \
IOCMD_MAKE(IOPROTO_FB, 8, _IOCMD_ACC_WR, 0)


//
// CLUTs
//

// Creates a new CLUT with 'entryCount' color entries.
// create_clut(size_t entryCount, int* _Nonnull pOutId)
#define VIO_CMD_CREATE_CLUT \
IOCMD_MAKE(IOPROTO_FB, 9, _IOCMD_ACC_WR, 0)

// Destroys the CLUT with id 'id'. Returns EBUSY if the CLUT is currently in use.
// destroy_clut(int id)
#define VIO_CMD_DESTROY_CLUT \
IOCMD_MAKE(IOPROTO_FB, 10, _IOCMD_ACC_WR, 0)

// Returns information about the CLUT 'id'.
// get_vio_clut_info(int id, vio_clut_info_t* _Nonnull pOutInfo)
#define VIO_CMD_CLUT_INFO \
IOCMD_MAKE(IOPROTO_FB, 11, _IOCMD_ACC_RD, 0)

// Updates the color entries if the CLUT 'id'. 'count' entries starting at index
// 'idx' are replaced with the color values stored in the array 'entries'.
// set_clut_entries(int id, size_t idx, size_t count, const vio_rgb32_t* _Nonnull entries)
#define VIO_CMD_SET_CLUT_ENTRIES \
IOCMD_MAKE(IOPROTO_FB, 12, _IOCMD_ACC_WR, 0)


//
// Sprites
//

typedef struct vio_sprite_caps {
    int minWidth, maxWidth;
    int minHeight, maxHeight;
    int lowSpriteNum, highSpriteNum;
    int xScale;     // One sprite pixel corresponds to 'xScale' screen pixels along the horizontal axis
    int yScale;
} vio_sprite_caps_t;

// Returns information about the limits of the sprite sub-system based on the
// currently active screen and mouse cursor configuration.
// get_sprite_info(sprite_info_t* _Nonnull info)
#define VIO_CMD_SPRITE_CAPS \
IOCMD_MAKE(IOPROTO_FB, 13, _IOCMD_ACC_RD, 0)

// Sets the position of a sprite. Note that sprites are only visible inside the
// screen aperture rectangle.
// set_sprite_position(int spriteId, int x, int y)
#define VIO_CMD_SET_SPRITE_POS \
IOCMD_MAKE(IOPROTO_FB, 14, _IOCMD_ACC_WR, 0)

// Shows or hides a sprite.
// set_sprite_visible(int spriteId, int isVisible)
#define VIO_CMD_SET_SPRITE_VIS \
IOCMD_MAKE(IOPROTO_FB, 15, _IOCMD_ACC_WR, 0)


//
// Screen
//

#define VIO_SCR_FRAMEBUFFER 1
#define VIO_SCR_CLUT        2
#define VIO_SCR_WIDTH       3
#define VIO_SCR_HEIGHT      4
#define VIO_SCR_PIXELFORMAT 5
#define VIO_SCR_END         0

// Configures the screen based on the given screen configuration. Pass NULL to
// turn video output off altogether.
// set_screen_config(const intptr_t* _Nullable config)
#define VIO_CMD_SET_SCREEN_CONFIG \
IOCMD_MAKE(IOPROTO_FB, 16, _IOCMD_ACC_WR, 0)


// Returns a copy of the currently active screen configuration. The configuration
// information is written to the provided buffer 'config' which is able to hold
// 'bufsiz' integer entries (sizeof(int)). EINVAL is returned if 'bufsiz' is 0.
// ERANGE is returned if 'bufsiz' is greater 0 but not big enough to hold all
// configuration information plus the terminated VIO_SCR_END entry. The
// returned configuration will contain the following configuration keys:
// VIO_SCR_FRAMEBUFFER
// VIO_SCR_CLUT (if the pixel format is one of the indirect formats)
// VIO_SCR_WIDTH
// VIO_SCR_HEIGHT
// VIO_SCR_PIXELFORMAT
// VIO_SCR_END
// int get_screen_config(intptr_t* _Nonnull config, size_t bufsiz)
#define VIO_CMD_SCREEN_CONFIG \
IOCMD_MAKE(IOPROTO_FB, 17, _IOCMD_ACC_RD, 0)

#endif /* _KPI_FRAMEBUFFER_H */

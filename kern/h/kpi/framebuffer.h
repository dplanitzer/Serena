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
#include <stdint.h>
#include <kpi/ioctl.h>
#include <kpi/types.h>

// Image pixel formats
#define GD_COLOR_INDEX1 1  // planar indexed with 1 bit per pixel
#define GD_COLOR_INDEX2 2  // planar indexed with 2 bits per pixel
#define GD_COLOR_INDEX3 3  // planar indexed with 3 bits per pixel
#define GD_COLOR_INDEX4 4  // planar indexed with 4 bits per pixel
#define GD_COLOR_INDEX5 5  // planar indexed with 5 bits per pixel
#define GD_COLOR_INDEX6 6  // planar indexed with 6 bits per pixel
#define GD_COLOR_INDEX7 7  // planar indexed with 7 bits per pixel
#define GD_COLOR_INDEX8 8  // planar indexed with 8 bits per pixel

#ifdef MACHINE_AMIGA
#define GD_RGB_HAM_6    10  // planar RGB Hold-And-Modify mode with 6 planes
#define GD_RGB_EHB_6    11  // planar RGB Extra-Half-Bright mode with 6 planes
#define GD_RGB_SPRITE_2 12  // interleaved planar indexed RGB with 2 planes
#endif
typedef int gd_pixfmt_t;


// Pre-defined image names
#define GD_FRONT_BUFFER 1


// Geometry and pixel encoding of a pixel buffer
typedef struct gd_image_info {
    int         width;
    int         height;
    gd_pixfmt_t pixelFormat;
} gd_image_info_t;


// Specifies what you want to do with the pixels when you call gdcMapImage()
#define GD_MAP_RDONLY  0
#define GD_MAP_RW      1


// Provides access to the pixels of a pixel buffer
typedef struct gd_image_data {
    void* _Nonnull  plane[8];
    size_t          planeCount;
    size_t          bytesPerRow;
} gd_image_data_t;


//
// Colors
//

// 32bit opaque RGB color
typedef unsigned int gd_rgb32_t;


// Returns a packed 32bit RGB color value
#define GD_RGB32_MAKE(__r, __g, __b) \
    ((((__r) & 0xff) << 16) | (((__g) & 0xff) << 8) | ((__b) & 0xff))

// Returns the red component of a RGB32
#define GD_RGB32_RED(__clr) \
    (((__clr) >> 16) & 0xff)

// Returns the green component of a RGB32
#define GD_RGB32_GREEN(__clr) \
    (((__clr) >> 8) & 0xff)

// Returns the blue component of a RGB32
#define GD_RGB32_BLUE(__clr) \
    ((__clr) & 0xff)


#define GD_RGB32_BLACK  0xff000000
#define GD_RGB32_WHITE  0xffffffff


//
// Images
//

// Creates a 2d image with size 'width' x 'height' pixels and pixel format
// 'format'. Returns the unique image ID in 'pOutId' on success. Note
// that the image width and height have to be > 1. The image may be directly
// mapped into the address space of the owning process or manipulated with the
// Blitter.
// gdCreateImage(int width, int height, gd_pixfmt_t format, int* _Nonnull pOutId)
#define GDC_CREATE_IMAGE \
IOCMD_MAKE(IOPROTO_FB, 1, _IOCMD_ACC_WR, 0)

// Destroys the image with id 'id'. Returns EBUSY if the image is currently
// mapped. Automatically unbinds the image if it is bound to a sprite and binds
// the sprite target to a null image. Does nothing if 'id' is 0.
// gdcDestroyImage(int id)
#define GDC_DESTROY_IMAGE \
IOCMD_MAKE(IOPROTO_FB, 2, _IOCMD_ACC_WR, 0)

// Returns information about the image with ID 'id'.
// gdcGetImageInfo(int id, gd_image_info_t* _Nonnull pOutInfo)
#define GDC_GET_IMAGE_INFO \
IOCMD_MAKE(IOPROTO_FB, 3, _IOCMD_ACC_RD, 0)

// Maps the pixels of the image 'id' into the address space of the calling
// process to allow direct access to the pixel data. 'mode' specifies
// whether the pixel data should be mapped for reading only or reading and
// writing. Returns with 'pOutMapping' filled in.
// gdcMapImage(int id, int mode, gd_image_data_t* _Nonnull pOutMapping)
#define GDC_MAP_IMAGE \
IOCMD_MAKE(IOPROTO_FB, 4, _IOCMD_ACC_RDWR, 0)

// Unmaps the pixels of the image 'id' and revokes access to the pixels.
// gdcUnmapImage(int id)
#define GDC_UNMAP_IMAGE \
IOCMD_MAKE(IOPROTO_FB, 5, _IOCMD_ACC_RDWR, 0)


//
// Sprites
//

// Acquires 'count' sprites with ascending priorities and returns the ids in
// the array 'pOutSpriteIds'. The array must have 'count' entries.
// If 'basePriority' is >= 0 then 'count' sprites with consecutive ascending
// priorities are allocated. Each sprite has a priority +1 greater than the
// previous sprite. The allocation will only succeed if 'count' consecutive
// sprite priorities are available. Otherwise the allocation fails and no sprites
// are allocated.
// 'If 'basePriority' is < 0 then 'count' sprites with an arbitrary and available
// priority are allocated.
// EAGAIN is returned if not enough sprites are available.
// gdcAcquireSprites(int basePriority, size_t count, int* _Nonnull pOutSpriteIds)
#define GDC_ACQUIRE_SPRITES \
IOCMD_MAKE(IOPROTO_FB, 6, _IOCMD_ACC_WR, 0)

// Releases 'count' sprites owned by the calling process. The sprite ids are
// provided by the 'spriteIds' array. All sprites currently owned by the calling
// process are released if 'spriteIds' is NULL.
// gdcReleaseSprites(const int* _Nullable spriteIds, size_t count)
#define GDC_RELEASE_SPRITES \
IOCMD_MAKE(IOPROTO_FB, 7, _IOCMD_ACC_WR, 0)

typedef struct gd_sprite_constraints {
    int minWidth, maxWidth;
    int minHeight, maxHeight;
    int maxPriority;
    int xShift;         // Convert display coordinates to sprite coordinates by shifting display pixels by 'xShift'/'yShift' to get sprite coordinates
    int yShift;
} gd_sprite_constraints_t;

// Returns information about the limits of the sprite sub-system based on the
// currently active display.
// gdcGetSpriteConstraints(gd_sprite_constraints_t* _Nonnull constraints)
#define GDC_SPRITE_CONSTRAINTS \
IOCMD_MAKE(IOPROTO_FB, 10, _IOCMD_ACC_RD, 0)


//
// Display
//

#define GD_APPLY    0
#define GD_CHECK    1

#define GD_CAP_GENLOCK          1
#define GD_CAP_DOUBLE_BUFFERED  2
#define GD_CAP_SPRITES          4
#define GD_CAP_CURSOR           8

typedef struct gd_display_mode {
	int	        width;
	int	        height;
	int	        refreshRate;
	gd_pixfmt_t pixelFormat;
} gd_display_mode_t;

typedef struct gd_display_params {
	gd_pixfmt_t overlayPixelFormat;
	int	        panWidth;
	int	        panHeight;
	int         overscanWidth;
	int         overscanHeight;
	int         capabilities;   // [requires genlock, requires double buffering, requires sprites, requires mouse cursor]
} gd_display_params_t;


// gdGetDisplayInfo() flavors
#define GD_DISPLAY_MODE     1
#define GD_DISPLAY_PARAMS   2

typedef void* gd_display_info_ref_t;


// Switches the display to the display mode 'mode' and applies the dynamic
// display parameters 'params' if specified. The switch is executed on the next
// VBL and the caller is blocked until teh switch has completed. 'op' specifies
// how teh switch should be executed:
// GD_APPLY - the switch is executed.
// GD_CHECK - the call verifies whether the switch would succeed but it does not
//            actually execute it. EOK is returned if the switch would be successful
//            and a suitable error is returned if it would fail.
// gdDisplayMode(const gd_display_mode_t* _Nonnull mode, const gd_display_params* _Nullable params, int op)
#define GDC_DISPLAY_MODE \
IOCMD_MAKE(IOPROTO_FB, 11, _IOCMD_ACC_WR, 0)

// Returns the display info indicated by 'flavor'.
// gdGetDisplayInfo(int flavor, gd_display_info_ref _Nonnull pOutInfo)
#define GDC_GET_DISPLAY_INFO \
IOCMD_MAKE(IOPROTO_FB, 12, _IOCMD_ACC_RD, 0)

// Returns the display mode with index 'index'. Returns EOK if a display mode
// with such an index exists and EINVAL if not. Call this function with index 0
// as teh first index and then with previous index + 1 until it returns EINVAL
// to get all supported display modes.
// gdEnumDisplayModes(int index, gd_display_mode_t* _Nonnull pOutMode)
#define GDC_ENUM_DISPLAY_MODES \
IOCMD_MAKE(IOPROTO_FB, 13, _IOCMD_ACC_RD, 0)


//
// CLUT
//

typedef struct gd_clut_info {
    size_t  entryCount;
    size_t  redBits;
    size_t  greenBits;
    size_t  blueBits;
} gd_clut_info_t;

// Replaces the color entries of the CLUT starting at entry 'idx' up to entry
// 'idx + count'. The provided color values are converted to the color resolution
// that is actually supported by the CLUT. The color values will become visible
// on the screen starting with the next VBL.
// gdClut(size_t idx, size_t count, const gd_rgb32_t* _Nonnull entries)
#define GDC_CLUT \
IOCMD_MAKE(IOPROTO_FB, 14, _IOCMD_ACC_WR, 0)

// Returns a copy of the CLUT entries from 'idx' to 'idx + count'. The returned
// color values represent the physical CLUT color values. They may have reduced
// color precision compared to the color values that were originally set by a
// gdClut() command. Use gdGetClutInfo() receive information about the
// supported CLUT color resolution.
// gdGetClut(size_t idx, size_t count, gd_rgb32_t* _Nonnull entries)
#define GDC_GET_CLUT \
IOCMD_MAKE(IOPROTO_FB, 15, _IOCMD_ACC_RD, 0)

// Returns information about the display CLUT. The number of color entries and
// the physical color resolution is returned.
// gdGetClutInfo(gd_clut_info_t* _Nonnull info)
#define GDC_GET_CLUT_INFO \
IOCMD_MAKE(IOPROTO_FB, 16, _IOCMD_ACC_RD, 0)



//
// Command Buffers
//

// Common command set
#define GD_OPCODE_NOP          0       // gd_opcode_t
#define GD_OPCODE_END          1       // gd_opcode_t

// Blit command set
#define GD_OPCODE_CLEAR_PIXELS 100     // struct gd_op_clear_pixels

// Transfer command set
#define GD_OPCODE_WRITE_PIXELS 200     // struct gd_op_write_pixels

// Sprite command set
#define GD_OPCODE_SPRITE_MOVE   300     // struct gd_op_sprite_move
#define GD_OPCODE_SPRITE_SHOW   301     // struct gd_op_sprite_show
#define GD_OPCODE_SPRITE_IMAGE    302     // struct gd_op_sprite_image 

typedef unsigned short gd_opcode_t;


// Blit command set
struct gd_op_clear_pixels {
    gd_opcode_t opcode;
    int         dstImageId;
};


// Transfer command set
struct gd_op_write_pixels {
    gd_opcode_t             opcode;
    int                     dstImageId;
    gd_pixfmt_t             format;
    size_t                  bytesPerRow;
    const void* _Nonnull    plane[1];   // 'n' plane pointers follow here where 'n' depends on 'format'
};


// Sprite command set
struct gd_op_sprite_image {
    gd_opcode_t opcode;
    int         spriteId;
    int         imageId;
};

struct gd_op_sprite_move {
    gd_opcode_t opcode;
    int         spriteId;
    int16_t     x;
    int16_t     y;
};

struct gd_op_sprite_show {
    gd_opcode_t opcode;
    int         spriteId;
    int16_t     visible;    // 'visible' != 0 -> show; otherwise hide
};


union vio_op {
    gd_opcode_t opcode;

    // Blit
    struct gd_op_clear_pixels   clear_pixels;

    // Transfer
    struct gd_op_write_pixels   write_pixels;

    // Sprite
    struct gd_op_sprite_image   sprite_image;
    struct gd_op_sprite_move    sprite_move;
    struct gd_op_sprite_show    sprite_show;
};


typedef struct gd_cmdbuf_desc {
    void* _Nonnull  addr;
    size_t          size;
    int             id;
} gd_cmdbuf_desc_t;

// Allocates a command buffer and maps it for reading and writing into the
// address space of the calling process. 'size' is the requested size of the
// buffer. The call returns the base address and the actual buffer size in
// 'desc' when successful. Note that the actual size may be greater than the
// requested size. However it will never be smaller.
// create_cmdbuf(size_t byteSize, const gd_cmdbuf_desc_t* _Nullable desc) -> id
#define GDC_CREATE_CMDBUF \
IOCMD_MAKE(IOPROTO_FB, 17, _IOCMD_ACC_WR, 0)

// Deallocates the command buffer 'id'.
// destroy_cmdbuf(int id)
#define GDC_DESTROY_CMDBUF \
IOCMD_MAKE(IOPROTO_FB, 18, _IOCMD_ACC_WR, 0)

#define GD_BLIT_QUEUE       0       // 2d blit/render commands
#define GD_TRANSFER_QUEUE   256     // copying data from/to RAM and a GD buffer
#define GD_SPRITE_QUEUE     512     // 2d sprite engine

// Submits the command buffer 'cmds_id' to the command queue 'queue_id' for
// execution. Depending on the targeted queue, the command may be executed
// asynchronously or synchronously. Malformed commands are ignored and execution
// is terminated if either an end command is encountered or a malformed command
// is encountered from which the command queue can not recover.
// gdSubmitCommands(int queue_id, int cmds_id)
#define GDC_SUBMIT_CMDBUF \
IOCMD_MAKE(IOPROTO_FB, 19, _IOCMD_ACC_WR, 0)

#endif /* _KPI_FRAMEBUFFER_H */

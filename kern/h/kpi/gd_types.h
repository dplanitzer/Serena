//
//  kpi/gd_types.h
//  kpi
//
//  Created by Dietmar Planitzer on 2/2/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_GD_TYPES_H
#define _KPI_GD_TYPES_H 1

#include <_cmndef.h>
#include <stdint.h>
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


typedef struct gd_sprite_constraints {
    int minWidth, maxWidth;
    int minHeight, maxHeight;
    int maxPriority;
    int xShift;         // Convert display coordinates to sprite coordinates by shifting display pixels by 'xShift'/'yShift' to get sprite coordinates
    int yShift;
} gd_sprite_constraints_t;


// gdDisplayMode() apply options
#define GD_APPLY    0
#define GD_CHECK    1

typedef struct gd_display_mode {
	int	        width;
	int	        height;
	int	        refreshRate;
	gd_pixfmt_t pixelFormat;
} gd_display_mode_t;


#define GD_CAP_GENLOCK          1
#define GD_CAP_DOUBLE_BUFFERED  2
#define GD_CAP_SPRITES          4
#define GD_CAP_CURSOR           8

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


// gdGetClutInfo()
typedef struct gd_clut_info {
    size_t  entryCount;
    size_t  redBits;
    size_t  greenBits;
    size_t  blueBits;
} gd_clut_info_t;



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


// gdSubmitCommandBuffer() command queues
#define GD_BLIT_QUEUE       0       // 2d blit/render commands
#define GD_TRANSFER_QUEUE   256     // copying data from/to RAM and a GD buffer
#define GD_SPRITE_QUEUE     512     // 2d sprite engine

#endif /* _KPI_GD_TYPES_H */

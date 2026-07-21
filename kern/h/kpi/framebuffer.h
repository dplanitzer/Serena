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

// Pixel buffer formats
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


// Pixel buffer binding targets
#define GD_SPRITE_0 0x20000
#define GD_SPRITE_1 0x20001
#define GD_SPRITE_2 0x20002
#define GD_SPRITE_3 0x20003
#define GD_SPRITE_4 0x20004
#define GD_SPRITE_5 0x20005
#define GD_SPRITE_6 0x20006
#define GD_SPRITE_7 0x20007


// Geometry and pixel encoding of a pixel buffer
typedef struct gd_buffer_info {
    int         width;
    int         height;
    gd_pixfmt_t pixelFormat;
} gd_buffer_info_t;


// Specifies what you want to do with the pixels when you call map_buffer()
#define GD_MAP_RDONLY  0
#define GD_MAP_RW      1


// Provides access to the pixels of a pixel buffer
typedef struct gd_buffer_data {
    void* _Nonnull  plane[8];
    size_t          planeCount;
    size_t          bytesPerRow;
} gd_buffer_data_t;


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
// Pixel buffers
//

// Creates a 2d pixel buffer of size 'width' x 'height' pixels and with a pixel
// encoding 'encoding' and returns the unique id of the buffer in 'pOutId'. Note
// that the buffer width and height have to be > 1. The buffer may be used to
// create a screen and it may be directly mapped into the address space of the
// owning process or manipulated with the Blitter.
// create_buffer(int width, int height, gd_pixfmt_t pixelFormat, int* _Nonnull pOutId)
#define GDC_CREATE_BUFFER \
IOCMD_MAKE(IOPROTO_FB, 1, _IOCMD_ACC_WR, 0)

// Destroys the buffer with id 'id'. Returns EBUSY if the buffer is currently
// mapped or is attached to a screen. Automatically unbinds the buffer if it is
// attached to a sprite and binds the sprite target to a null buffer. Does
// nothing if 'id' is 0.
// destroy_buffer(int id)
#define GDC_DESTROY_BUFFER \
IOCMD_MAKE(IOPROTO_FB, 2, _IOCMD_ACC_WR, 0)

// Returns information about the buffer 'id'.
// get_gd_buffer_info(int id, gd_buffer_info_t* _Nonnull pOutInfo)
#define GDC_BUFFER_INFO \
IOCMD_MAKE(IOPROTO_FB, 3, _IOCMD_ACC_RD, 0)

// Maps the backing store of the buffer 'id' into the address space of the
// calling process to allow direct access to the pixel data. 'mode' specifies
// whether the pixel data should be mapped for reading only or reading and
// writing. Returns with 'pOutMapping' filled in.
// map_buffer(int id, int mode, gd_buffer_data_t* _Nonnull pOutMapping)
#define GDC_MAP_BUFFER \
IOCMD_MAKE(IOPROTO_FB, 4, _IOCMD_ACC_RDWR, 0)

// Unmaps the backing store of the buffer 'id' and revokes access to the pixels.
// unmap_buffer(int id)
#define GDC_UNMAP_BUFFER \
IOCMD_MAKE(IOPROTO_FB, 5, _IOCMD_ACC_RDWR, 0)

// Synchronously executes the pixel buffer command buffer 'cmds_id' starting at
// its base address plus 'offset' and continuing until the first encountered end
// instruction. Blocks the caller until all instructions have been executed.
// Execution ends prematurely if an instruction with invalid arguments is
// encountered and the detected error is returned. The buffer 'buf_id' is the
// target of all render operations.
// gdBufferCommands(int buf_id, int cmds_id, size_t offset)
#define GDC_BUFFER_COMMANDS \
IOCMD_MAKE(IOPROTO_FB, 6, _IOCMD_ACC_WR, 0)


//
// Sprites
//

typedef struct gd_sprite_caps {
    int minWidth, maxWidth;
    int minHeight, maxHeight;
    int lowSpriteNum, highSpriteNum;
    int xScale;     // One sprite pixel corresponds to 'xScale' screen pixels along the horizontal axis
    int yScale;
} gd_sprite_caps_t;

// Returns information about the limits of the sprite sub-system based on the
// currently active screen and mouse cursor configuration.
// get_sprite_info(sprite_info_t* _Nonnull info)
#define GDC_SPRITE_CAPS \
IOCMD_MAKE(IOPROTO_FB, 7, _IOCMD_ACC_RD, 0)


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

typedef struct gd_display_buffers {
	int front_left;
	int	front_right;
	int back_left;
	int back_right;
} gd_display_buffers_t;


#define GD_DISPLAY_MODE     1
#define GD_DISPLAY_PARAMS   2
#define GD_DISPLAY_BUFFERS  3

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
IOCMD_MAKE(IOPROTO_FB, 10, _IOCMD_ACC_WR, 0)

// Returns the display info indicated by 'flavor'.
// gdGetDisplayInfo(int flavor, gd_display_info_ref _Nonnull pOutInfo)
#define GDC_GET_DISPLAY_INFO \
IOCMD_MAKE(IOPROTO_FB, 14, _IOCMD_ACC_RD, 0)

// Returns the display mode with index 'index'. Returns EOK if a display mode
// with such an index exists and EINVAL if not. Call this function with index 0
// as teh first index and then with previous index + 1 until it returns EINVAL
// to get all supported display modes.
// gdEnumDisplayModes(int index, gd_display_mode_t* _Nonnull pOutMode)
#define GDC_ENUM_DISPLAY_MODES \
IOCMD_MAKE(IOPROTO_FB, 15, _IOCMD_ACC_RD, 0)

// Executes commands from the command buffer 'id', starting at offset 'offset'
// until an end command is encountered. All commands target the display and are
// scheduled such that they will update the display on the next VBL. Execution
// ends at the first encountered end command or if an error is encountered.
// gdDisplayCommands(int id, size_t offset)
#define GDC_DISPLAY_COMMANDS \
IOCMD_MAKE(IOPROTO_FB, 11, _IOCMD_ACC_WR, 0)


//
// CLUT
//

typedef struct gd_clut_info {
    size_t  entryCount;
    size_t  redBits;
    size_t  greenBits;
    size_t  blueBits;
} gd_clut_info_t;

// Returns a copy of the CLUT entries from 'idx' to 'idx + count'. The returned
// color values represent the physical CLUT color values. They may have reduced
// color precision compared to the color values that were originally set by a
// gdCmdClut() command. Use gdGetClutInfo() receive information about the
// supported CLUT color resolution.
// gdGetClut(size_t idx, size_t count, gd_rgb32_t* _Nonnull entries)
#define GDC_GET_CLUT \
IOCMD_MAKE(IOPROTO_FB, 8, _IOCMD_ACC_RD, 0)

// Returns information about the display CLUT. The number of color entries and
// the physical color resolution is returned.
// gdGetClutInfo(gd_clut_info_t* _Nonnull info)
#define GDC_GET_CLUT_INFO \
IOCMD_MAKE(IOPROTO_FB, 9, _IOCMD_ACC_RD, 0)



//
// Command Buffers
//

// Common command set
#define GD_OPCODE_NOP          0       // gd_opcode_t
#define GD_OPCODE_END          1       // gd_opcode_t

// 2d render command set
#define GD_OPCODE_DRAW_PIXELS  100     // struct gd_op_draw_pixels
#define GD_OPCODE_CLEAR_PIXELS 101     // gd_opcode_t

// Display command set
#define GD_OPCODE_CLUT_RGB32   200     // struct gd_op_clut_rgb32
#define GD_OPCODE_PUT_SPRITE   201     // struct gd_op_put_sprite
#define GD_OPCODE_SHOW_SPRITE  202     // struct gd_op_show_sprite
#define GD_OPCODE_BIND_BUFFER  203     // struct gd_op_bind_buffer 

typedef unsigned short gd_opcode_t;


// 2d render command set
struct gd_op_draw_pixels {
    gd_opcode_t             opcode;
    gd_pixfmt_t             format;
    size_t                  bytesPerRow;
    const void* _Nonnull    plane[1];   // 'n' plane pointers follow here where 'n' depends on 'format'
};


// Display command set
struct gd_op_bind_buffer {
    gd_opcode_t    opcode;
    int             target;
    int             bufferId;
};

struct gd_op_clut_rgb32 {
    gd_opcode_t     opcode;
    uint16_t        idx;
    uint16_t        count;
    gd_rgb32_t      color[1];       // 'count' color entries follow here
};

struct gd_op_put_sprite {
    gd_opcode_t     opcode;
    int             spriteId;
    int16_t         x;
    int16_t         y;
};

struct gd_op_show_sprite {
    gd_opcode_t     opcode;
    int             spriteId;
    int16_t         visible;    // 'visible' != 0 -> show; otherwise hide
};

union vio_op {
    gd_opcode_t                 opcode;
    struct gd_op_draw_pixels    draw_pixels;

    struct gd_op_bind_buffer    bind_buffer;
    struct gd_op_clut_rgb32     clut_rgb32;
    struct gd_op_put_sprite     put_sprite;
    struct gd_op_show_sprite    show_sprite;
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
IOCMD_MAKE(IOPROTO_FB, 12, _IOCMD_ACC_WR, 0)

// Deallocates the command buffer 'id'.
// destroy_cmdbuf(int id)
#define GDC_DESTROY_CMDBUF \
IOCMD_MAKE(IOPROTO_FB, 13, _IOCMD_ACC_WR, 0)

#endif /* _KPI_FRAMEBUFFER_H */

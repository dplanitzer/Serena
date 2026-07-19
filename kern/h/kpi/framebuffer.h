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

// Returns information about the buffer 'id'.
// get_vio_buffer_info(int id, vio_buffer_info_t* _Nonnull pOutInfo)
#define VIO_CMD_BUFFER_INFO \
IOCMD_MAKE(IOPROTO_FB, 3, _IOCMD_ACC_RD, 0)

// Maps the backing store of the buffer 'id' into the address space of the
// calling process to allow direct access to the pixel data. 'mode' specifies
// whether the pixel data should be mapped for reading only or reading and
// writing. Returns with 'pOutMapping' filled in.
// map_buffer(int id, int mode, vio_buffer_data_t* _Nonnull pOutMapping)
#define VIO_CMD_MAP_BUFFER \
IOCMD_MAKE(IOPROTO_FB, 4, _IOCMD_ACC_RDWR, 0)

// Unmaps the backing store of the buffer 'id' and revokes access to the pixels.
// unmap_buffer(int id)
#define VIO_CMD_UNMAP_BUFFER \
IOCMD_MAKE(IOPROTO_FB, 5, _IOCMD_ACC_RDWR, 0)

// Synchronously executes the pixel buffer command buffer 'cmds_id' starting at
// its base address plus 'offset' and continuing until the first encountered end
// instruction. Blocks the caller until all instructions have been executed.
// Execution ends prematurely if an instruction with invalid arguments is
// encountered and the detected error is returned. The buffer 'buf_id' is the
// target of all render operations.
// gdBufferCommands(int buf_id, int cmds_id, size_t offset)
#define VIO_CMD_BUFFER_COMMANDS \
IOCMD_MAKE(IOPROTO_FB, 15, _IOCMD_ACC_WR, 0)


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
IOCMD_MAKE(IOPROTO_FB, 6, _IOCMD_ACC_RD, 0)


//
// Framebuffer
//

// Creates a new framebuffer. Note that you must attach a pixel buffer to the
// framebuffer before it can be set as the current framebuffer.
// create_framebuffer(size_t entryCount, int* _Nonnull pOutId)
#define VIO_CMD_CREATE_FRAMEBUFFER \
IOCMD_MAKE(IOPROTO_FB, 7, _IOCMD_ACC_WR, 0)

// Destroys the framebuffer 'id'. EBUSY is returned if 'id' is the current
// framebuffer. You must first remove it as the current framebuffer before you
// can destroy it.
// destroy_framebuffer(int id)
#define VIO_CMD_DESTROY_FRAMEBUFFER \
IOCMD_MAKE(IOPROTO_FB, 8, _IOCMD_ACC_WR, 0)

// Attaches the pixel buffer 'buf_id' to the framebuffer 'fb_id' as a front
// buffer. An already attached buffer is first detached. Pass 0 for 'buf_id' to
// simply detach the currently attach buffer without attaching a new one.
// Returns EBUSY if the framebuffer 'fb_id' is the current framebuffer since hot
// swapping of buffers is not supported.
// attach_buffer(int fb_id, int buf_id)
#define VIO_CMD_ATTACH_BUFFER \
IOCMD_MAKE(IOPROTO_FB, 9, _IOCMD_ACC_WR, 0)

// Returns information about the framebuffer 'id'.
// get_framebuffer_info(int id, vio_clut_info_t* _Nonnull pOutInfo)
#define VIO_CMD_FRAMEBUFFER_INFO \
IOCMD_MAKE(IOPROTO_FB, 10, _IOCMD_ACC_RD, 0)

// Sets the framebuffer 'id' as the current framebuffer. The framebuffer must
// have a pixel buffer with a suitable width, height and pixel format attached
// to it. The video mode is automatically selected based on the framebuffer and
// pixel buffer configuration. Pass 0 as the 'id' to turn video off altogether.
// set_current_framebuffer(int id)
#define VIO_CMD_SET_CURRENT_FRAMEBUFFER \
IOCMD_MAKE(IOPROTO_FB, 11, _IOCMD_ACC_WR, 0)


// Returns the id of the current framebuffer. 0 is returned if no framebuffer is
// current and video is off.
// get_current_framebuffer(int* _Nonnull pOutId)
#define VIO_CMD_GET_CURRENT_FRAMEBUFFER \
IOCMD_MAKE(IOPROTO_FB, 12, _IOCMD_ACC_RD, 0)


//
// Screen
//

// Synchronously executes the screen command buffer 'id' start at its base
// address plus 'offset' and continuing until the first encountered end
// instruction. Blocks the caller until all instructions have been executed.
// Execution ends prematurely if an instruction with invalid arguments is
// encountered and the detected error is returned.
// gdScreenCommands(int id, size_t offset)
#define VIO_CMD_SCREEN_COMMANDS \
IOCMD_MAKE(IOPROTO_FB, 16, _IOCMD_ACC_WR, 0)



//
// Command Buffers
//

// Core instructions subset
#define VIO_OPCODE_NOP          0       // vio_opcode_t
#define VIO_OPCODE_END          1       // vio_opcode_t

// Pixel buffer instructions subset
#define VIO_OPCODE_DRAW_PIXELS  100     // struct vio_op_draw_pixels
#define VIO_OPCODE_CLEAR_PIXELS 101     // vio_opcode_t

// Screen instructions subset
#define VIO_OPCODE_CLUT_RGB32   200     // struct vio_op_clut_rgb32
#define VIO_OPCODE_PUT_SPRITE   201     // struct vio_op_put_sprite
#define VIO_OPCODE_SHOW_SPRITE  202     // struct vio_op_show_sprite
#define VIO_OPCODE_BIND_BUFFER  203     // struct vio_op_bind_buffer 

typedef unsigned short vio_opcode_t;


// Pixel buffer instruction subset
struct vio_op_draw_pixels {
    vio_opcode_t            opcode;
    vio_pixfmt_t            format;
    size_t                  bytesPerRow;
    const void* _Nonnull    plane[1];   // 'n' plane pointers follow here where 'n' depends on 'format'
};


// Screen instructions subset
struct vio_op_bind_buffer {
    vio_opcode_t    opcode;
    int             target;
    int             bufferId;
};

struct vio_op_clut_rgb32 {
    vio_opcode_t    opcode;
    int             clutId;
    uint16_t        idx;
    uint16_t        count;
    vio_rgb32_t     color[1];       // 'count' color entries follow here
};

struct vio_op_put_sprite {
    vio_opcode_t    opcode;
    int             spriteId;
    int16_t         x;
    int16_t         y;
};

struct vio_op_show_sprite {
    vio_opcode_t    opcode;
    int             spriteId;
    int16_t         visible;    // 'visible' != 0 -> show; otherwise hide
};

union vio_op {
    vio_opcode_t                opcode;
    struct vio_op_draw_pixels   draw_pixels;

    struct vio_op_bind_buffer   bind_buffer;
    struct vio_op_clut_rgb32    clut_rgb32;
    struct vio_op_put_sprite    put_sprite;
    struct vio_op_show_sprite   show_sprite;
};


typedef struct vio_cmdbuf_desc {
    void* _Nonnull  addr;
    size_t          size;
    int             id;
} vio_cmdbuf_desc_t;

// Allocates a command buffer and maps it for reading and writing into the
// address space of the calling process. 'size' is the requested size of the
// buffer. The call returns the base address and the actual buffer size in
// 'desc' when successful. Note that the actual size may be greater than the
// requested size. However it will never be smaller.
// create_cmdbuf(size_t byteSize, const vio_cmdbuf_desc_t* _Nullable desc) -> id
#define VIO_CMD_CREATE_CMDBUF \
IOCMD_MAKE(IOPROTO_FB, 13, _IOCMD_ACC_WR, 0)

// Deallocates the command buffer 'id'.
// destroy_cmdbuf(int id)
#define VIO_CMD_DESTROY_CMDBUF \
IOCMD_MAKE(IOPROTO_FB, 14, _IOCMD_ACC_WR, 0)

#endif /* _KPI_FRAMEBUFFER_H */

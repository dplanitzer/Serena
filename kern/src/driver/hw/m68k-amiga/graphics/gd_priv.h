//
//  gd_priv.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/25/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _GD_PRIV_H
#define _GD_PRIV_H

#include "gd.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <ext/queue.h>
#include <ext/try.h>
#include <hal/hw/m68k-amiga/chipset.h>
#include <sched/mtx.h>
#include <sched/waitqueue.h>
#include "video_conf.h"

// Memory management model for resources that are used by a Copper program:
// *) A user may request the deletion of a resource that's currently being used
//    by a Copper program.
// *) In this case, the resource is immediately removed from the resource specific
//    ID table (this makes the resource immediately no longer accessible be the
//    user). The id is then removed from the resource (set to 0).
// *) Once the Copper program is retired, the Copper manager checks whether the
//    resource id == 0, if so it destroys the resource; otherwise it leaves it
//    alive.


// Image
#define GD_DYNAMIC_IMAGE_BASE   0x100   // Images allocated at runtime. < than this are well known image

#define GD_IMAGE_MAPPED 2   // Image is mapped

typedef struct image {
    deque_node_t        chain;
    int                 id;
    pid_t               ownerPid;
    int                 refCount;
    uint8_t* _Nullable  plane[8];   // plane[0] is the buffer base pointer used for kfree(); plane[1..] are internal pointers into this buffer
    int                 width;
    int                 height;
    size_t              bytesPerRow;
    gd_pixfmt_t         pixelFormat;
    int8_t              planeCount;
    uint8_t             flags;
} image_t;

#define _gdRetainImage(__self) \
(((image_t*)(__self))->refCount++)

extern void _gdReleaseImage(image_t* _Nullable self);

extern void _gdPublishImage(image_t* _Nonnull self, int id);
extern void _gdConcealImage(image_t* _Nonnull self);
extern image_t* _Nullable _gdGetImageById(pid_t pid, int id);
extern errno_t _gdWritePixels(struct image* self, const void* _Nonnull planes[], size_t bytesPerRow, gd_pixfmt_t format);


#define _gdGetImageId(__self) \
(((image_t*)(__self))->id)

// Returns the pixel width of the surface.
#define _gdGetImageWidth(__self) \
((__self)->width)

// Returns the pixel height of the surface.
#define _gdGetImageHeight(__self) \
((__self)->height)

// Returns the number of planes in the surface.
#define _gdGetImagePlaneCount(__self) \
((__self)->planeCount)

// Returns the bytes-per-row value
#define _gdGetImageBytesPerRow(__self) \
((__self)->bytesPerRow)

// Returns the pixel format
#define _gdGetImagePixelFormat(__self) \
((__self)->pixelFormat)

// Returns the n-th plane of the surface.
#define _gdGetImagePlane(__self, __idx) \
((__self)->plane[__idx])

// Returns true if the surface is currently mapped
#define _gdIsImageMapped(__self) \
(((__self)->flags & GD_IMAGE_MAPPED) == GD_IMAGE_MAPPED)


// Sprite channel
typedef struct sprite_channel {
    image_t* _Nullable  image;      // Image holding the sprite image data and control words
    pid_t               ownerPid;
    int16_t             x;
    int16_t             y;
    bool                isVisible;
    char                id;
} sprite_channel_t;

extern uint32_t _calc_sprite_ctl(const sprite_channel_t* _Nonnull self);
extern bool _bind_sprite_image(sprite_channel_t* _Nonnull spr, image_t* _Nullable pbo);
extern void _sprite_image_or_visibility_changed(const sprite_channel_t* _Nonnull spr);

// Submit a change to the control word of the sprite 'spridx'. The new control
// word will be written to the sprite data pointer 'sprptr'. The actual write
// will happen in the next VBL interrupt.
extern void sprite_ctl_submit(int spridx, void* _Nonnull sprptr, uint32_t ctl);

// Cancels a previously submitted sprite control word update.
extern void sprite_ctl_cancel(int spridx);


// Cursor
extern errno_t _gdInitCursor(void);


// Copper program instruction
typedef uint32_t  copper_instr_t;

#define COP_MOVE(reg, val)              (((reg) << 16) | (val))
#define COP_WAIT_MASKED(vp, hp, ve, he) ((((vp & 0x00ff) << 24) | (((hp & 0x007f) << 17) | 0x10000)) | (((ve & 0x007f) << 8) | (((he & 0x007f) << 1) | 0x8000)))
#define COP_WAIT(vp, hp)                COP_WAIT_MASKED(vp, hp, 0x7f, 0x7f)
#define COP_END()                       0xfffffffe


// Copper program state
#define COP_STATE_IDLE      0
#define COP_STATE_READY     1
#define COP_STATE_RUNNING   2
#define COP_STATE_RETIRED   3


typedef struct copper_res {
    image_t* _Nonnull   spr[SPRITE_COUNT];
} copper_res_t;


typedef struct copper_locs {
    int16_t     bplcon0;
    int16_t     clut;
    int16_t     sprptr;
} copper_locs_t;


// A Copper program consists of at least an odd field sub-program and optionally
// an additional even field sub-program. We assume that both field programs have
// the same number of instructions and are pretty much identical except for the
// differences that are required to make the fields work correctly.
// A Copper program declares its dependencies on surfaces and color tables. It
// holds a use on these resources while it is alive.
struct copper_prog {
    struct copper_prog* _Nullable   next;
    
    copper_instr_t* _Nonnull        prog;
    size_t                          prog_size;  // program segment size in terms of #instructions

    copper_instr_t* _Nonnull        odd_entry;  // odd field entry point has to exist
    copper_instr_t* _Nullable       even_entry; // event field entry point only exists for interlaced programs

    volatile int8_t                 state;
    int8_t                          reserved[3];

    copper_locs_t                   loc;        // locations of instructions that may be edited

    copper_res_t                    res;
};
typedef struct copper_prog* copper_prog_t;


extern errno_t copper_prog_create(size_t instr_count, copper_prog_t _Nullable * _Nonnull pOutProg);

// Frees the given Copper program.
extern void copper_prog_destroy(copper_prog_t _Nullable prog);

// Calculates how many instructions are needed for a Copper program for the given
// video configuration.
extern size_t calc_copper_prog_instruction_count(const video_conf_t* _Nonnull vc);

// Compiles the odd (and needed the even) field program(s) for the given video
// configuration, framebuffer, CLUT and sprite configuration and writes the
// instructions to the given Copper program. Note that the Copper program must
// be big enough to hold all instructions.
extern void copper_prog_compile(copper_prog_t _Nonnull self, const video_conf_t* _Nonnull vc, image_t* _Nullable pFrontBuffer);


// Schedules the provided Copper program. This program will start running at the
// beginning of the next video frame. Pass COPFLAG_WAIT_RUNNING to wait until the
// new program has started running.
#define COPFLAG_WAIT_RUNNING    1
extern void copper_schedule(copper_prog_t _Nonnull prog, unsigned flags);

// Removes the currently scheduled ready Copper program and returns it.
extern copper_prog_t _Nullable copper_unschedule(void);


// The currently running Copper program.
extern copper_prog_t _Nonnull   g_copper_running_prog;


// These functions edit the given Copper program to update it to reflect changes
// to the program dependencies or the general graphic driver environment 
extern void copper_prog_set_lp_enabled(copper_prog_t self, bool isEnabled);
extern void copper_prog_clut_changed(copper_prog_t _Nonnull self, size_t startIdx, size_t count);
extern void copper_prog_sprptr_changed(copper_prog_t _Nonnull self, int spridx, image_t* _Nullable pbo);


#define MAX_CACHED_COPPER_PROGS 4
#define MOUSE_SPRITE_PRI 0
#define CLUT_SIZE   32

extern uint16_t* _Nonnull       g_null_sprite_data;
extern sprite_channel_t         g_sprite[SPRITE_COUNT];
extern bool                     g_light_pen_enabled;
extern uint16_t                 g_clut[CLUT_SIZE];
extern uint8_t                  g_clut_size;
extern image_t*                 g_cur_front_buffer;
extern const video_conf_t*      g_cur_video_config;

extern errno_t _gdInitCopper(void);

// Creates the even and odd field Copper programs for the given screen. There will
// always be at least an odd field program. The even field program will only exist
// for an interlaced screen.
extern errno_t create_screen_copper_prog(const video_conf_t* _Nonnull vc, image_t* _Nonnull pFrontBuffer, copper_prog_t _Nullable * _Nonnull pOutProg);

extern copper_prog_t _Nullable copper_get_editable_prog(void);

#endif /* _GD_PRIV_H */

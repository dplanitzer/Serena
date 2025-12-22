//
//  copper.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/25/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _COPPER_H
#define _COPPER_H

#include <kern/errno.h>
#include <kern/types.h>
#include <machine/hw/m68k-amiga/chipset.h>
#include <sched/vcpu.h>
#include "ColorTable.h"
#include "Surface.h"
#include "video_conf.h"


// Copper program instruction
typedef uint32_t  copper_instr_t;

#define COP_MOVE(reg, val)          (((reg) << 16) | (val))
#define COP_WAIT(vp, hp, ve, he)    ((((vp & 0x00ff) << 24) | (((hp & 0x007f) << 17) | 0x10000)) | (((ve & 0x007f) << 8) | (((he & 0x007f) << 1) | 0x8000)))
#define COP_END()                   0xfffffffe


typedef struct sprite_channel {
    Surface* _Nullable  surface;    // Surface holding the sprite image data and control words
    int                 x;
    int                 y;
    bool                isVisible;
} sprite_channel_t;


// Copper program state
#define COP_STATE_IDLE      0
#define COP_STATE_READY     1
#define COP_STATE_RUNNING   2
#define COP_STATE_RETIRED   3


typedef struct copper_res {
    ColorTable* _Nonnull    clut;
    Surface* _Nullable      fb;

    Surface* _Nonnull       spr[SPRITE_COUNT];
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

    const video_conf_t* _Nonnull    video_conf;
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
extern void copper_prog_compile(copper_prog_t _Nonnull self, const video_conf_t* _Nonnull vc, Surface* _Nullable fb, ColorTable* _Nonnull clut, const sprite_channel_t _Nonnull spr[], Surface* _Nonnull nullSpriteSurface, bool isLightPenEnabled);


// Initializes the Copper scheduler. 'prog' is the bootstrap Copper program. This
// program will start running as soon as the bottom-most line of the current
// video frame has been reached.  
extern errno_t copper_init(copper_prog_t _Nonnull prog, int signo, vcpu_t _Nullable sigvp);

// Starts the Copper scheduling services running.
extern void copper_start(void);

// Removes the next program from the retired Copper program list. Returns NULL
// if there are no retired programs.
extern copper_prog_t _Nullable copper_acquire_retired_prog(void);

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
extern void copper_prog_sprptr_changed(copper_prog_t _Nonnull self, int spridx, Surface* _Nonnull srf);


// Submit a change to the control word of the sprite 'spridx'. The new control
// word will be written to the sprite data pointer 'sprptr'. The actual write
// will happen in the next VBL interrupt.
extern void sprite_ctl_submit(int spridx, void* _Nonnull sprptr, uint32_t ctl);

// Cancels a previously submitted sprite control word update.
extern void sprite_ctl_cancel(int spridx);

#endif /* _COPPER_H */

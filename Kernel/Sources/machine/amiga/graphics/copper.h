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
#include <machine/amiga/chipset.h>
#include <sched/vcpu.h>
#include "ColorTable.h"
#include "Surface.h"
#include "video_conf.h"


// Copper program instruction
typedef uint32_t  copper_instr_t;

#define COP_MOVE(reg, val)  (((reg) << 16) | (val))
#define COP_END()           0xfffffffe


// Copper program state
#define COP_STATE_IDLE      0
#define COP_STATE_READY     1
#define COP_STATE_RUNNING   2
#define COP_STATE_RETIRED   3


typedef struct copper_res {
    GObject* _Nullable  fb;
    GObject* _Nullable  clut;
} copper_res_t;


typedef struct copper_locs {
    int16_t     bplcon0;
    int16_t     clut;
    int16_t     sprptr;
} copper_locs_t;


#define COPED_SPRPTR    1
#define COPED_CLUT      2

#define COPED_SPRPTR_SENTINEL   0xffffffff

typedef struct copper_edits {
    uint8_t     pending;
    uint8_t     reserved[3];

    int16_t     clut_low_idx;
    int16_t     clut_high_idx;

    uint32_t    sprptr[SPRITE_COUNT+1];   // 31..8: sprite dma pointer; 7..0: sprite number (0xff -> marks end of list)
} copper_edits_t;


struct copper_prog {
    struct copper_prog* _Nullable   next;
    
    copper_instr_t* _Nonnull        prog;
    size_t                          prog_size;  // program segment size in terms of #instructions

    copper_instr_t* _Nonnull        odd_entry;  // odd field entry point has to exist
    copper_instr_t* _Nullable       even_entry; // event field entry point only exists for interlaced programs

    volatile int8_t                 state;
    int8_t                          reserved[3];

    copper_locs_t                   loc;        // locations of instructions that may be edited
    copper_edits_t                  ed;         // pending Copper program edits

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
extern void copper_prog_compile(copper_prog_t _Nonnull self, const video_conf_t* _Nonnull vc, Surface* _Nullable fb, ColorTable* _Nonnull clut, uint16_t* _Nonnull sprdma[], bool isLightPenEnabled);

// Clear pending edits
#define copper_prog_clear_edits(__self) \
(__self)->ed.pending = 0; \
(__self)->ed.clut_low_idx = 0; \
(__self)->ed.clut_high_idx = 0; \
(__self)->ed.sprptr[0] = COPED_SPRPTR_SENTINEL


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

// The currently running Copper program.
extern copper_prog_t _Nonnull   g_copper_running_prog;


// Changes the light pen enabled/disabled state of the currently running Copper
// program.
extern void copper_cur_set_lp_enabled(bool isEnabled);

extern void copper_cur_set_sprptr(int spridx, uint16_t* _Nonnull sprptr);
extern void copper_cur_set_clut_range(size_t idx, size_t count);

#endif /* _COPPER_H */

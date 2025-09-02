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
#include <sched/vcpu.h>
#include "GObject.h"


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


struct copper_prog {
    struct copper_prog* _Nullable   next;
    
    copper_instr_t* _Nonnull        prog;
    size_t                          prog_size;  // program segment size in terms of #instructions

    copper_instr_t* _Nonnull        odd_entry;  // odd field entry point has to exist
    copper_instr_t* _Nullable       even_entry; // event field entry point only exists for interlaced programs

    volatile int8_t                 state;
    int8_t                          reserved[3];

    copper_res_t                    res;
};
typedef struct copper_prog* copper_prog_t;


extern errno_t copper_prog_create(size_t instr_count, copper_prog_t _Nullable * _Nonnull pOutProg);

// Frees the given Copper program.
extern void copper_prog_destroy(copper_prog_t _Nullable prog);


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

#endif /* _COPPER_H */

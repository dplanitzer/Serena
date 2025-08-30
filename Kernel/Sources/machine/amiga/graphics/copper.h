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


// Copper program instruction
typedef uint32_t  copper_instr_t;

#define COP_MOVE(reg, val)  (((reg) << 16) | (val))
#define COP_END()           0xfffffffe


// Copper program state
#define COP_STATE_IDLE      0
#define COP_STATE_READY     1
#define COP_STATE_RUNNING   2
#define COP_STATE_RETIRED   3


struct copper_prog {
    struct copper_prog* _Nullable   next;
    
    copper_instr_t* _Nonnull        prog;
    size_t                          prog_size;  // program segment size in terms of #instructions

    copper_instr_t* _Nonnull        odd_entry;  // odd field entry point has to exist
    copper_instr_t* _Nullable       even_entry; // event field entry point only exists for interlaced programs

    volatile int8_t                 state;
    int8_t                          reserved[3];
};
typedef struct copper_prog* copper_prog_t;


extern errno_t copper_prog_create(size_t instr_count, copper_prog_t _Nullable * _Nonnull pOutProg);

extern errno_t copper_init(uint16_t* _Nonnull nullSpriteData);
extern void copper_start(void);

#define COPFLAG_WAIT_RUNNING    1
extern void copper_schedule(copper_prog_t _Nullable prog, unsigned flags);

#endif /* _COPPER_H */

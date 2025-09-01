//
//  copper_comp.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/27/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _COPPER_COMP_H
#define _COPPER_COMP_H

#include <kern/errno.h>
#include <kern/types.h>
#include "copper.h"
#include "ColorTable.h"
#include "Sprite.h"
#include "Surface.h"


typedef struct copper_params {
    Surface* _Nonnull       fb;
    ColorTable* _Nonnull    clut;
    uint16_t** _Nonnull     sprdma;
    bool                    isHires;
    bool                    isLace;
    bool                    isPal;
    bool                    isLightPenEnabled;
} copper_params_t;


// Computes the size of a Copper program. The size is given in terms of the
// number of Copper instruction words.
extern size_t copper_comp_calclength(const copper_params_t* _Nonnull params);

// Compiles a screen refresh Copper program into the given buffer (which must be
// big enough to store the program).
// \return a pointer to where the next instruction after the program would go 
extern copper_instr_t* _Nonnull copper_comp_compile(copper_instr_t* _Nonnull ip, const copper_params_t* _Nonnull params, bool isOddField);

#endif /* _COPPER_COMP_H */

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
#include "VideoConfiguration.h"
#include "Screen.h"
#include "Sprite.h"
#include "Surface.h"


// Computes the size of a Copper program. The size is given in terms of the
// number of Copper instruction words.
extern size_t copper_comp_calclength(Screen* _Nonnull scr);

// Compiles a screen refresh Copper program into the given buffer (which must be
// big enough to store the program).
// \return a pointer to where the next instruction after the program would go 
extern copper_instr_t* _Nonnull copper_comp_compile(copper_instr_t* _Nonnull ip, Screen* _Nonnull scr, Sprite* sprite[SPRITE_COUNT], Sprite* _Nonnull nullSprite, Sprite* _Nullable mouseCursor, bool isLightPenEnabled, bool isOddField);

#endif /* _COPPER_COMP_H */

//
//  CopperProgram.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/24/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "CopperProgram.h"
#include "Screen.h"


// Computes the size of a Copper program. The size is given in terms of the
// number of Copper instruction words.
static size_t cop_screen_refresh_prog_size(Screen* _Nonnull pScreen)
{
    Surface* pFramebuffer = pScreen->framebuffer;

    return 3                                // BPLCON0, BPLCON1, BPLCON2
            + 2                             // DIWSTART, DIWSTOP
            + 2                             // DDFSTART, DDFSTOP
            + 2                             // BPL1MOD, BPL2MOD
            + 2 * pFramebuffer->planeCount  // BPLxPT[nplanes]
            + 2 * NUM_HARDWARE_SPRITES      // SPRxPT
            + 1;                            // DMACON
}

// Compiles a screen refresh Copper program into the given buffer (which must be
// big enough to store the program).
// \return a pointer to where the next instruction after the program would go 
static CopperInstruction* _Nonnull cop_make_screen_refresh_prog(CopperInstruction* _Nonnull pCode, Screen* _Nonnull pScreen, bool isLightPenEnabled, bool isOddField)
{
    const ScreenConfiguration* pConfig = pScreen->screenConfig;
    const uint32_t firstLineByteOffset = isOddField ? 0 : pConfig->ddf_mod;
    const uint16_t lpen_bit = isLightPenEnabled ? BPLCON0F_LPEN : 0;
    Surface* pFramebuffer = pScreen->framebuffer;
    register CopperInstruction* ip = pCode;
    
    // BPLCONx
    *ip++ = COP_MOVE(BPLCON0, pConfig->bplcon0 | lpen_bit | ((uint16_t)pFramebuffer->planeCount & 0x07) << 12);
    *ip++ = COP_MOVE(BPLCON1, 0);
    *ip++ = COP_MOVE(BPLCON2, 0x0024);
    
    // DIWSTART / DIWSTOP
    *ip++ = COP_MOVE(DIWSTART, (pConfig->diw_start_v << 8) | pConfig->diw_start_h);
    *ip++ = COP_MOVE(DIWSTOP, (pConfig->diw_stop_v << 8) | pConfig->diw_stop_h);
    
    // DDFSTART / DDFSTOP
    *ip++ = COP_MOVE(DDFSTART, pConfig->ddf_start);
    *ip++ = COP_MOVE(DDFSTOP, pConfig->ddf_stop);
    
    // BPLxMOD
    *ip++ = COP_MOVE(BPL1MOD, pConfig->ddf_mod);
    *ip++ = COP_MOVE(BPL2MOD, pConfig->ddf_mod);
    
    // BPLxPT
    for (int i = 0, r = BPL_BASE; i < pFramebuffer->planeCount; i++, r += 4) {
        const uint32_t bplpt = (uint32_t)(pFramebuffer->plane[i]) + firstLineByteOffset;
        
        *ip++ = COP_MOVE(r + 0, (bplpt >> 16) & UINT16_MAX);
        *ip++ = COP_MOVE(r + 2, bplpt & UINT16_MAX);
    }

    // SPRxPT
    uint16_t dmaf_sprite = 0;
    for (int i = 0, r = SPRITE_BASE; i < NUM_HARDWARE_SPRITES; i++, r += 4) {
        Sprite* sprite;

        if (pScreen->sprite[i]) {
            sprite = pScreen->sprite[i];
            dmaf_sprite = DMACONF_SPREN;
        }
        else {
            sprite = pScreen->nullSprite;
        }


        const uint32_t sprpt = (uint32_t)sprite->data;
        *ip++ = COP_MOVE(r + 0, (sprpt >> 16) & UINT16_MAX);
        *ip++ = COP_MOVE(r + 2, sprpt & UINT16_MAX);
    }

    // DMACON
    *ip++ = COP_MOVE(DMACON, DMACONF_SETCLR | DMACONF_BPLEN | dmaf_sprite | DMACONF_DMAEN);

    return ip;
}

// Compiles a Copper program to display a non-interlaced screen or a single
// field of an interlaced screen.
errno_t CopperProgram_CreateScreenRefresh(Screen* _Nonnull pScreen, bool isLightPenEnabled, bool isOddField, CopperProgram* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    const size_t nFrameInstructions = cop_screen_refresh_prog_size(pScreen);
    const size_t nInstructions = nFrameInstructions + 1;
    CopperProgram* self;
    
    try(kalloc_options(sizeof(CopperProgram) + (nInstructions - 1) * sizeof(CopperInstruction), KALLOC_OPTION_UNIFIED, (void**) &self));
    SListNode_Init(&self->node);
    CopperInstruction* ip = self->entry;

    ip = cop_make_screen_refresh_prog(ip, pScreen, isLightPenEnabled, isOddField);

    // end instructions
    *ip = COP_END();
    
    *pOutSelf = self;
    return EOK;
    
catch:
    *pOutSelf = NULL;
    return err;
}

// Frees the given Copper program.
void CopperProgram_Destroy(CopperProgram* _Nullable self)
{
    kfree(self);
}

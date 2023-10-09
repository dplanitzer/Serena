//
//  Copper.c
//  Apollo
//
//  Created by Dietmar Planitzer on 9/24/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "GraphicsDriverPriv.h"

////////////////////////////////////////////////////////////////////////////////
// MARK:
// MARK: Copper program compiler
////////////////////////////////////////////////////////////////////////////////

// Computes the size of a Copper program. The size is given in terms of the
// number of Copper instruction words.
Int CopperCompiler_GetScreenRefreshProgramInstructionCount(Screen* _Nonnull pScreen)
{
    Surface* pFramebuffer = pScreen->framebuffer;

    return 3                                // BPLCONx
            + 2                             // DIWSTART, DIWSTOP
            + 2                             // DDFSTART, DDF_STOP
            + 2                             // BPLxMOD
            + 2 * pFramebuffer->planeCount  // BPLxPT[nplanes]
            + 16                            // SPRxPT
            + 1;                            // DMACON
}

// Compiles a screen refresh Copper program into the given buffer (which must be
// big enough to store the program).
// \return a pointer to where the next instruction after the program would go 
CopperInstruction* _Nonnull CopperCompiler_CompileScreenRefreshProgram(CopperInstruction* _Nonnull pCode, Screen* _Nonnull pScreen, Bool isLightPenEnabled, Bool isOddField)
{
    const ScreenConfiguration* pConfig = pScreen->screenConfig;
    const UInt32 firstLineByteOffset = isOddField ? 0 : pConfig->ddf_mod;
    const UInt16 lpen_bit = isLightPenEnabled ? BPLCON0F_LPEN : 0x0000;
    Surface* pFramebuffer = pScreen->framebuffer;
    register CopperInstruction* ip = pCode;
    
    // BPLCONx
    *ip++ = COP_MOVE(BPLCON0, pConfig->bplcon0 | lpen_bit | ((UInt16)pFramebuffer->planeCount & 0x07) << 12);
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
    for (Int i = 0, r = BPL_BASE; i < pFramebuffer->planeCount; i++, r += 4) {
        const UInt32 bplpt = (UInt32)(pFramebuffer->planes[i]) + firstLineByteOffset;
        
        *ip++ = COP_MOVE(r + 0, (bplpt >> 16) & UINT16_MAX);
        *ip++ = COP_MOVE(r + 2, bplpt & UINT16_MAX);
    }

    // SPRxPT
    for (Int i = 0, r = SPRITE_BASE; i < NUM_HARDWARE_SPRITES; i++, r += 4) {
        const UInt32 sprpt = (UInt32)pScreen->sprite[i]->data;

        *ip++ = COP_MOVE(r + 0, (sprpt >> 16) & UINT16_MAX);
        *ip++ = COP_MOVE(r + 2, sprpt & UINT16_MAX);
    }

    // DMACON
    const UInt16 dmaf_sprite = (pScreen->spritesInUseCount > 0) ? DMAF_SPRITE : 0;
    *ip++ = COP_MOVE(DMACON, DMAF_SETCLR | DMAF_RASTER | dmaf_sprite | DMAF_MASTER);

    return ip;
}

// Compiles a Copper program to display a non-interlaced screen or a single
// field of an interlaced screen.
ErrorCode CopperProgram_CreateScreenRefresh(Screen* _Nonnull pScreen, Bool isLightPenEnabled, Bool isOddField, CopperProgram* _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    const Int nFrameInstructions = CopperCompiler_GetScreenRefreshProgramInstructionCount(pScreen);
    const Int nInstructions = nFrameInstructions + 1;
    CopperProgram* pProg;
    
    try(kalloc_options(sizeof(CopperProgram) + (nInstructions - 1) * sizeof(CopperInstruction), KALLOC_OPTION_UNIFIED, (Byte**) &pProg));
    CopperInstruction* ip = pProg->entry;

    ip = CopperCompiler_CompileScreenRefreshProgram(ip, pScreen, isLightPenEnabled, isOddField);

    // end instructions
    *ip = COP_END();
    
    *pOutProg = pProg;
    return EOK;
    
catch:
    return err;
}

// Frees the given Copper program.
void CopperProgram_Destroy(CopperProgram* _Nullable pProg)
{
    kfree((Byte*)pProg);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Copper Scheduler
////////////////////////////////////////////////////////////////////////////////

void CopperScheduler_Init(CopperScheduler* _Nonnull pScheduler)
{
    pScheduler->readyEvenFieldProg = NULL;
    pScheduler->readyOddFieldProg = NULL;
    pScheduler->runningEvenFieldProg = NULL;
    pScheduler->runningOddFieldProg = NULL;
    pScheduler->flags = 0;
}

void CopperScheduler_Deinit(CopperScheduler* _Nonnull pScheduler)
{
    // Nothing to do for now
}

// Schedules the given odd and even field Copper programs for execution. The
// programs will start executing at the next vertical blank. Expects at least
// an odd field program if the current video mode is non-interlaced and both
// and odd and an even field program if the video mode is interlaced. The video
// display is turned off if the odd field program is NULL.
void CopperScheduler_ScheduleProgram(CopperScheduler* _Nonnull pScheduler, const CopperProgram* _Nullable pOddFieldProg, const CopperProgram* _Nullable pEvenFieldProg)
{
    const Int irs = cpu_disable_irqs();
    pScheduler->readyEvenFieldProg = pEvenFieldProg;
    pScheduler->readyOddFieldProg = pOddFieldProg;
    pScheduler->flags |= COPF_CONTEXT_SWITCH_REQ;
    cpu_restore_irqs(irs);
}

// Called when the Copper scheduler has received a request to switch to a new
// Copper program. Updates the running program, retires the old program, updates
// the Copper state and triggers the first run of the Copper program
static void CopperScheduler_ContextSwitch(CopperScheduler* _Nonnull pScheduler)
{
    CHIPSET_BASE_DECL(cp);

    // Move the scheduled program to running state. But be sure to first
    // turn off the Copper and raster DMA. Then move the data. Then turn the
    // Copper DMA back on if we have a prog. The program is responsible for 
    // turning the raster DMA on.
    *CHIPSET_REG_16(cp, DMACON) = (DMAF_COPPER | DMAF_RASTER | DMAF_SPRITE);
    pScheduler->runningEvenFieldProg = pScheduler->readyEvenFieldProg;
    pScheduler->runningOddFieldProg = pScheduler->readyOddFieldProg;
    pScheduler->flags &= ~COPF_CONTEXT_SWITCH_REQ;


    // No odd field prog means that we should leave video turned off altogether
    if (pScheduler->runningOddFieldProg == NULL) {
        return;
    }


    // Interlaced if we got an odd & even field program
    if (pScheduler->runningEvenFieldProg) {
        pScheduler->flags |= COPF_INTERLACED;
    } else {
        pScheduler->flags &= ~COPF_INTERLACED;
    }


    // Install the correct program in the Copper, re-enable DMA and trigger
    // a jump to the program
    if ((pScheduler->flags & COPF_INTERLACED) != 0) {
        // Handle interlaced (dual field) programs. Which program to activate depends
        // on whether the current field is the even or the odd one
        const UInt16 isLongFrame = *CHIPSET_REG_16(cp, VPOSR) & 0x8000;

        if (isLongFrame) {
            // Odd field
            *CHIPSET_REG_32(cp, COP1LC) = (UInt32) pScheduler->runningOddFieldProg->entry;
        } else {
            // Even field
            *CHIPSET_REG_32(cp, COP1LC) = (UInt32) pScheduler->runningEvenFieldProg->entry;
        }
    } else {
        *CHIPSET_REG_32(cp, COP1LC) = (UInt32) pScheduler->runningOddFieldProg->entry;
    }

    *CHIPSET_REG_16(cp, COPJMP1) = 0;
    *CHIPSET_REG_16(cp, DMACON) = (DMAF_SETCLR | DMAF_COPPER | DMAF_MASTER);
}

// Called at the vertical blank interrupt. Triggers the execution of the correct
// Copper program (odd or even field as needed). Also makes a scheduled program
// active / running if needed.
void CopperScheduler_Run(CopperScheduler* _Nonnull pScheduler)
{
    CHIPSET_BASE_DECL(cp);

    // Check whether a new program is scheduled to run. If so move it to running
    // state
    if ((pScheduler->flags & COPF_CONTEXT_SWITCH_REQ) != 0) {
        CopperScheduler_ContextSwitch(pScheduler);
        return;
    }

    
    // Jump to the field dependent Copper program if we are in interlace mode.
    // Nothing to do if we are in non-interlaced mode
    if ((pScheduler->flags & COPF_INTERLACED) != 0) {
        const UInt16 isLongFrame = *CHIPSET_REG_16(cp, VPOSR) & 0x8000;

        if (isLongFrame) {
            // Odd field
            *CHIPSET_REG_32(cp, COP1LC) = (UInt32) pScheduler->runningOddFieldProg->entry;
        } else {
            // Even field
            *CHIPSET_REG_32(cp, COP1LC) = (UInt32) pScheduler->runningEvenFieldProg->entry;
        }
        *CHIPSET_REG_16(cp, COPJMP1) = 0;
    }
}

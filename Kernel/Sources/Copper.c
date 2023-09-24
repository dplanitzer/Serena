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
void CopperCompiler_CompileScreenRefreshProgram(CopperInstruction* _Nonnull pCode, Screen* _Nonnull pScreen, Bool isOddField)
{
    static const UInt8 BPLxPTH[MAX_PLANE_COUNT] = {BPL1PTH, BPL2PTH, BPL3PTH, BPL4PTH, BPL5PTH, BPL6PTH};
    static const UInt8 BPLxPTL[MAX_PLANE_COUNT] = {BPL1PTL, BPL2PTL, BPL3PTL, BPL4PTL, BPL5PTL, BPL6PTL};
    const VideoConfiguration* pConfig = pScreen->videoConfig;
    const UInt32 firstLineByteOffset = isOddField ? 0 : pConfig->ddf_mod;
    const UInt16 lpen_mask = pScreen->isLightPenEnabled ? 0x0008 : 0x0000;
    Surface* pFramebuffer = pScreen->framebuffer;
    Int ip = 0;
    
    // BPLCONx
    pCode[ip++] = COP_MOVE(BPLCON0, pConfig->bplcon0 | lpen_mask | ((UInt16)pFramebuffer->planeCount & 0x07) << 12);
    pCode[ip++] = COP_MOVE(BPLCON1, 0);
    pCode[ip++] = COP_MOVE(BPLCON2, 0x0024);
    
    // DIWSTART / DIWSTOP
    pCode[ip++] = COP_MOVE(DIWSTART, (pConfig->diw_start_v << 8) | pConfig->diw_start_h);
    pCode[ip++] = COP_MOVE(DIWSTOP, (pConfig->diw_stop_v << 8) | pConfig->diw_stop_h);
    
    // DDFSTART / DDFSTOP
    pCode[ip++] = COP_MOVE(DDFSTART, pConfig->ddf_start);
    pCode[ip++] = COP_MOVE(DDFSTOP, pConfig->ddf_stop);
    
    // BPLxMOD
    pCode[ip++] = COP_MOVE(BPL1MOD, pConfig->ddf_mod);
    pCode[ip++] = COP_MOVE(BPL2MOD, pConfig->ddf_mod);
    
    // BPLxPT
    for (Int i = 0; i < pFramebuffer->planeCount; i++) {
        const UInt32 bplpt = (UInt32)(pFramebuffer->planes[i]) + firstLineByteOffset;
        
        pCode[ip++] = COP_MOVE(BPLxPTH[i], (bplpt >> 16) & UINT16_MAX);
        pCode[ip++] = COP_MOVE(BPLxPTL[i], bplpt & UINT16_MAX);
    }

    // SPRxPT
    const UInt32 spr32 = (UInt32)pScreen->mouseCursorSprite;
    const UInt16 sprH = (spr32 >> 16) & UINT16_MAX;
    const UInt16 sprL = spr32 & UINT16_MAX;
    const UInt32 nullspr = (const UInt32)pScreen->nullSprite;
    const UInt16 nullsprH = (nullspr >> 16) & UINT16_MAX;
    const UInt16 nullsprL = nullspr & UINT16_MAX;

    pCode[ip++] = COP_MOVE(SPR0PTH, nullsprH);
    pCode[ip++] = COP_MOVE(SPR0PTL, nullsprL);
    pCode[ip++] = COP_MOVE(SPR1PTH, nullsprH);
    pCode[ip++] = COP_MOVE(SPR1PTL, nullsprL);
    pCode[ip++] = COP_MOVE(SPR2PTH, nullsprH);
    pCode[ip++] = COP_MOVE(SPR2PTL, nullsprL);
    pCode[ip++] = COP_MOVE(SPR3PTH, nullsprH);
    pCode[ip++] = COP_MOVE(SPR3PTL, nullsprL);
    pCode[ip++] = COP_MOVE(SPR4PTH, nullsprH);
    pCode[ip++] = COP_MOVE(SPR4PTL, nullsprL);
    pCode[ip++] = COP_MOVE(SPR5PTH, nullsprH);
    pCode[ip++] = COP_MOVE(SPR5PTL, nullsprL);
    pCode[ip++] = COP_MOVE(SPR6PTH, nullsprH);
    pCode[ip++] = COP_MOVE(SPR6PTL, nullsprL);
    pCode[ip++] = COP_MOVE(SPR7PTH, sprH);
    pCode[ip++] = COP_MOVE(SPR7PTL, sprL);
    
    // DMACON
    pCode[ip++] = COP_MOVE(DMACON, DMAF_SETCLR | DMAF_RASTER | DMAF_MASTER);
    // XXX turned the mouse cursor off for now
//    pCode[ip++] = COP_MOVE(DMACON, DMAF_SETCLR | DMAF_RASTER | DMAF_SPRITE | DMAF_MASTER);
}

// Compiles a Copper program to display a non-interlaced screen or a single field
// of an interlaced screen.
ErrorCode CopperProgram_CreateScreenRefresh(Screen* _Nonnull pScreen, Bool isOddField, CopperInstruction* _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    Int ip = 0;
    const Int nFrameInstructions = CopperCompiler_GetScreenRefreshProgramInstructionCount(pScreen);
    const Int nInstructions = nFrameInstructions + 1;
    CopperInstruction* pCode;
    
    try(kalloc_options(nInstructions * sizeof(CopperInstruction), KALLOC_OPTION_UNIFIED, (Byte**) &pCode));
    
    CopperCompiler_CompileScreenRefreshProgram(&pCode[ip], pScreen, isOddField);
    ip += nFrameInstructions;

    // end instructions
    pCode[ip++] = COP_END();
    
    *pOutProg = pCode;
    return EOK;
    
catch:
    return err;
}

// Frees the given Copper program.
void CopperProgram_Destroy(CopperInstruction* _Nullable pCode)
{
    kfree((Byte*)pCode);
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
void CopperScheduler_ScheduleProgram(CopperScheduler* _Nonnull pScheduler, const CopperInstruction* _Nullable pOddFieldProg, const CopperInstruction* _Nullable pEvenFieldProg)
{
    const Int irs = cpu_disable_irqs();
    pScheduler->readyEvenFieldProg = pEvenFieldProg;
    pScheduler->readyOddFieldProg = pOddFieldProg;
    pScheduler->flags |= COPF_CONTEXT_SWITCH_REQ;
    cpu_restore_irqs(irs);
}

// Called at the vertical blank interrupt. Triggers the execution of the correct
// Copper program (odd or even field as needed). Also makes a scheduled program
// active / running if needed.
void CopperScheduler_ContextSwitch(CopperScheduler* _Nonnull pScheduler)
{
    CHIPSET_BASE_DECL(cp);

    // Check whether a new program is scheduled to run. If so move it to running
    // state
    if ((pScheduler->flags & COPF_CONTEXT_SWITCH_REQ) != 0) {
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
    }

    
    // Activate the correct Copper program based on whether the display mode is
    // interlaced or non-interlaced
    if ((pScheduler->flags & COPF_INTERLACED) != 0) {
        // Handle interlaced (dual field) programs. Which program to activate depends
        // on whether the current field is the even or the odd one
        const UInt16 isLongFrame = *CHIPSET_REG_16(cp, VPOSR) & 0x8000;

        if (isLongFrame) {
            // Odd field
            *CHIPSET_REG_32(cp, COP1LC) = (UInt32) pScheduler->runningOddFieldProg;
        } else {
            // Even field
            *CHIPSET_REG_32(cp, COP1LC) = (UInt32) pScheduler->runningEvenFieldProg;
        }
    } else {
        *CHIPSET_REG_32(cp, COP1LC) = (UInt32) pScheduler->runningOddFieldProg;
    }

    *CHIPSET_REG_16(cp, DMACON) = (DMAF_SETCLR | DMAF_COPPER | DMAF_MASTER);
    *CHIPSET_REG_16(cp, COPJMP1) = 0;
}

//
//  CopperScheduler.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/24/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "CopperScheduler.h"
#include <kern/assert.h>
#include <kern/timespec.h>
#include <machine/amiga/chipset.h>
#include <machine/irq.h>

static void CopperScheduler_GarbageCollectRetiredPrograms(CopperScheduler* _Nonnull self);


void CopperScheduler_Init(CopperScheduler* _Nonnull self)
{
    self->readyEvenFieldProg = NULL;
    self->readyOddFieldProg = NULL;
    self->runningEvenFieldProg = NULL;
    self->runningOddFieldProg = NULL;
    self->flags = 0;
    sem_init(&self->retirementSignaler, 0);
    SList_Init(&self->retiredProgs);
    try_bang(DispatchQueue_Create(0, 1, VCPU_QOS_UTILITY, VCPU_PRI_NORMAL, &self->retiredProgsCollector));
    try_bang(DispatchQueue_DispatchAsync(self->retiredProgsCollector, (VoidFunc_1)CopperScheduler_GarbageCollectRetiredPrograms, self));
}

void CopperScheduler_Deinit(CopperScheduler* _Nonnull self)
{
    Object_Release(self->retiredProgsCollector);
    self->retiredProgsCollector = NULL;
    sem_deinit(&self->retirementSignaler);
    SList_Deinit(&self->retiredProgs);
}

// Schedules the given odd and even field Copper programs for execution. The
// programs will start executing at the next vertical blank. Expects at least
// an odd field program if the current video mode is non-interlaced and both
// and odd and an even field program if the video mode is interlaced.
//
// !! Note that the odd and even field programs must be two separate programs.
// !! They can not be shared.
void CopperScheduler_ScheduleProgram(CopperScheduler* _Nonnull self, const CopperProgram* _Nonnull pOddFieldProg, const CopperProgram* _Nullable pEvenFieldProg)
{
    const unsigned sim = irq_set_mask(IRQ_MASK_VBLANK);
    self->readyEvenFieldProg = pEvenFieldProg;
    self->readyOddFieldProg = pOddFieldProg;
    self->flags |= COPF_CONTEXT_SWITCH_REQ;
    irq_set_mask(sim);
}

static void CopperScheduler_GarbageCollectRetiredPrograms(CopperScheduler* _Nonnull self)
{
    while (true) {
        int ignored;
        SListNode* pCur;

        sem_acquireall(&self->retirementSignaler, &TIMESPEC_INF, &ignored);

        const unsigned sim = irq_set_mask(IRQ_MASK_VBLANK);
        pCur = self->retiredProgs.first;
        self->retiredProgs.first = NULL;
        self->retiredProgs.last = NULL;
        irq_set_mask(sim);

        while (pCur) {
            SListNode* pNext = pCur->next;

            CopperProgram_Destroy((CopperProgram*)pCur);
            pCur = pNext;
        }
    }
}

// Called when the Copper scheduler has received a request to switch to a new
// Copper program. Updates the running program, retires the old program, updates
// the Copper state and triggers the first run of the Copper program
static void CopperScheduler_ContextSwitch(CopperScheduler* _Nonnull self)
{
    CHIPSET_BASE_DECL(cp);
    *CHIPSET_REG_16(cp, DMACON) = (DMACONF_COPEN | DMACONF_BPLEN | DMACONF_SPREN);

    // Retire the currently running program(s)
    bool doSignal = false;
    if (self->runningEvenFieldProg) {
        SList_InsertBeforeFirst(&self->retiredProgs, &self->runningEvenFieldProg->node);
        doSignal = true;
    }
    if (self->runningOddFieldProg) {
        SList_InsertBeforeFirst(&self->retiredProgs, &self->runningOddFieldProg->node);
        doSignal = true;
    }


    // Move the scheduled program to running state. But be sure to first
    // turn off the Copper and raster DMA. Then move the data. Then turn the
    // Copper DMA back on if we have a prog. The program is responsible for 
    // turning the raster DMA on.
    self->runningEvenFieldProg = self->readyEvenFieldProg;
    self->runningOddFieldProg = self->readyOddFieldProg;
    self->flags &= ~COPF_CONTEXT_SWITCH_REQ;


    // No odd field prog means that we should leave video turned off altogether
    if (self->runningOddFieldProg) {
        
        // Interlaced if we got an odd & even field program
        if (self->runningEvenFieldProg) {
            self->flags |= COPF_INTERLACED;
        } else {
            self->flags &= ~COPF_INTERLACED;
        }


        // Install the correct program in the Copper, re-enable DMA and trigger
        // a jump to the program
        if ((self->flags & COPF_INTERLACED) != 0) {
            // Handle interlaced (dual field) programs. Which program to activate depends
            // on whether the current field is the even or the odd one
            const uint16_t isLongFrame = *CHIPSET_REG_16(cp, VPOSR) & 0x8000;

            *CHIPSET_REG_32(cp, COP1LC) = (uint32_t)((isLongFrame) ? self->runningOddFieldProg->entry : self->runningEvenFieldProg->entry);
        } else {
            *CHIPSET_REG_32(cp, COP1LC) = (uint32_t)self->runningOddFieldProg->entry;
        }

        *CHIPSET_REG_16(cp, DMACON) = (DMACONF_SETCLR | DMACONF_COPEN | DMACONF_DMAEN);
        *CHIPSET_REG_16(cp, COPJMP1) = 0;
    }


    if (doSignal) {
        sem_relinquish_irq(&self->retirementSignaler);
    }
}

// Called at the vertical blank interrupt. Triggers the execution of the correct
// Copper program (odd or even field as needed). Also makes a scheduled program
// active / running if needed.
void CopperScheduler_Run(CopperScheduler* _Nonnull self)
{
    // Check whether a new program is scheduled to run. If so move it to running
    // state
    if ((self->flags & COPF_CONTEXT_SWITCH_REQ) != 0) {
        CopperScheduler_ContextSwitch(self);
        return;
    }

    
    // Jump to the field dependent Copper program if we are in interlace mode.
    // Nothing to do if we are in non-interlaced mode
    if ((self->flags & COPF_INTERLACED) != 0) {
        CHIPSET_BASE_DECL(cp);
        const uint16_t isLongFrame = *CHIPSET_REG_16(cp, VPOSR) & 0x8000;

        *CHIPSET_REG_32(cp, COP1LC) = (uint32_t)((isLongFrame) ? self->runningOddFieldProg->entry : self->runningEvenFieldProg->entry);
        *CHIPSET_REG_16(cp, COPJMP1) = 0;
    }
}

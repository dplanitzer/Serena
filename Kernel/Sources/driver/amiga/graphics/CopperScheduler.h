//
//  CopperScheduler.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef CopperScheduler_h
#define CopperScheduler_h

#include <dispatcher/Semaphore.h>
#include <dispatchqueue/DispatchQueue.h>
#include "CopperProgram.h"


#define COPF_CONTEXT_SWITCH_REQ (1 << 7)
#define COPF_INTERLACED         (1 << 6)
typedef struct CopperScheduler {
    const CopperProgram* _Nullable  readyOddFieldProg;
    const CopperProgram* _Nullable  readyEvenFieldProg;

    const CopperProgram* _Nullable  runningOddFieldProg;
    const CopperProgram* _Nullable  runningEvenFieldProg;

    uint32_t                        flags;

    Semaphore                       retirementSignaler;
    SList                           retiredProgs;
    DispatchQueueRef _Nonnull       retiredProgsCollector;
} CopperScheduler;

extern void CopperScheduler_Init(CopperScheduler* _Nonnull self);
extern void CopperScheduler_Deinit(CopperScheduler* _Nonnull self);
extern void CopperScheduler_ScheduleProgram(CopperScheduler* _Nonnull self, const CopperProgram* _Nonnull pOddFieldProg, const CopperProgram* _Nullable pEvenFieldProg);
extern void CopperScheduler_Run(CopperScheduler* _Nonnull self);

#endif /* CopperScheduler_h */

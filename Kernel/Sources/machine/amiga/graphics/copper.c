//
//  amiga/copper.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/25/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "copper.h"
#include <kern/kernlib.h>
#include <machine/irq.h>
#include <machine/amiga/chipset.h>
#include <sched/sem.h>

extern void copper_prog_apply_edits(copper_prog_t _Nonnull self, copper_instr_t* ep);
extern int copper_irq(void);


static irq_handler_t            g_copper_vblank;
static copper_prog_t _Nullable  g_copper_ready_prog;
copper_prog_t _Nonnull          g_copper_running_prog;
static copper_prog_t _Nullable  g_copper_retired_progs;
static sem_t                    g_copper_notify_sem;
static int                      g_retire_signo;
static vcpu_t _Nullable         g_retire_vcpu;
static int8_t                   g_copper_is_running_interlaced;


errno_t copper_init(copper_prog_t _Nonnull prog, int signo, vcpu_t _Nullable sigvp)
{
    decl_try_err();

    sem_init(&g_copper_notify_sem, 0);

    // Do a forced schedule of the null program
    g_copper_ready_prog = NULL;
    g_copper_running_prog = prog;
    g_copper_running_prog->state = COP_STATE_RUNNING;
    g_copper_is_running_interlaced = 0;
    g_retire_signo = signo;
    g_retire_vcpu = sigvp;

catch:
    return err;
}

void copper_start(void)
{
    // Let the Copper run our null program
    CHIPSET_BASE_DECL(cp);

    *CHIPSET_REG_16(cp, DMACON) = (DMACONF_COPEN | DMACONF_SPREN | DMACONF_BPLEN);
    chipset_wait_bof();
    *CHIPSET_REG_32(cp, COP1LC) = (uint32_t)g_copper_running_prog->odd_entry;
    *CHIPSET_REG_16(cp, COPJMP1) = 0;
    *CHIPSET_REG_16(cp, DMACON) = (DMACONF_SETCLR | DMACONF_COPEN | DMACONF_DMAEN);


    // Activate the Copper context switcher
    g_copper_vblank.id = IRQ_ID_VBLANK;
    g_copper_vblank.priority = IRQ_PRI_HIGHEST + 4;
    g_copper_vblank.enabled = true;
    g_copper_vblank.func = (irq_handler_func_t)copper_irq;
    g_copper_vblank.arg = NULL;

    irq_add_handler(&g_copper_vblank);
    irq_enable_src(IRQ_ID_VBLANK);
}

copper_prog_t _Nullable copper_acquire_retired_prog(void)
{
    const unsigned sim = irq_set_mask(IRQ_MASK_VBLANK);
    copper_prog_t prog = g_copper_retired_progs;

    if (prog) {
        g_copper_retired_progs = prog->next;
        prog->next = NULL;
    }

    irq_set_mask(sim);

    return prog;
}

static void _copper_prog_retire(copper_prog_t _Nonnull prog)
{
    if (g_copper_retired_progs) {
        prog->next = g_copper_retired_progs;
        g_copper_retired_progs = prog;
    }
    else {
        g_copper_retired_progs = prog;
        prog->next = NULL;
    }

    prog->state = COP_STATE_RETIRED;
}

void copper_schedule(copper_prog_t _Nullable prog, unsigned flags)
{
    const unsigned sim = irq_set_mask(IRQ_MASK_VBLANK);
    if (g_copper_ready_prog) {
        _copper_prog_retire(g_copper_ready_prog);
    }

    g_copper_ready_prog = prog;
    prog->state = COP_STATE_READY;
    irq_set_mask(sim);


    if ((flags & COPFLAG_WAIT_RUNNING) == COPFLAG_WAIT_RUNNING) {
        while (prog->state == COP_STATE_READY) {
            sem_acquire(&g_copper_notify_sem, &TIMESPEC_INF);
        }
    }
}

// Called when the Copper scheduler has received a request to switch to a new
// Copper program. Updates the running program, retires the old program, updates
// the Copper state and triggers the first run of the Copper program
static void copper_csw(void)
{
    CHIPSET_BASE_DECL(cp);
    *CHIPSET_REG_16(cp, DMACON) = (DMACONF_COPEN | DMACONF_BPLEN | DMACONF_SPREN);


    // Retire the currently running program
    _copper_prog_retire(g_copper_running_prog);


    // Move the scheduled program to running state. But be sure to first
    // turn off the Copper and raster DMA. Then move the data. Then turn the
    // Copper DMA back on if we have a prog. The program is responsible for 
    // turning the raster DMA on.
    g_copper_running_prog = g_copper_ready_prog;
    g_copper_running_prog->state = COP_STATE_RUNNING;
    g_copper_ready_prog = NULL;


    // Interlaced if we got an odd & even field program
    g_copper_is_running_interlaced = (g_copper_running_prog->even_entry != NULL);


    // Install the correct program in the Copper, re-enable DMA and trigger
    // a jump to the program
    if (g_copper_is_running_interlaced) {
        // Handle interlaced (dual field) programs. Which program to activate depends
        // on whether the current field is the even or the odd one
        const uint16_t isLongFrame = *CHIPSET_REG_16(cp, VPOSR) & 0x8000;

        *CHIPSET_REG_32(cp, COP1LC) = (uint32_t)((isLongFrame) ? g_copper_running_prog->odd_entry : g_copper_running_prog->even_entry);
    } else {
        *CHIPSET_REG_32(cp, COP1LC) = (uint32_t)g_copper_running_prog->odd_entry;
    }

    *CHIPSET_REG_16(cp, COPJMP1) = 0;
    *CHIPSET_REG_16(cp, DMACON) = (DMACONF_SETCLR | DMACONF_COPEN | DMACONF_DMAEN);


    if (g_retire_vcpu) {
        vcpu_sigsend_irq(g_retire_vcpu, g_retire_signo, false);
    }
    sem_relinquish_irq(&g_copper_notify_sem);
}

// Called at the vertical blank interrupt. Triggers the execution of the correct
// Copper program (odd or even field as needed). Also makes a scheduled program
// active / running if needed.
int copper_irq(void)
{
    bool doClearEdits = false;

    // Check whether a new program is scheduled to run. If so move it to running
    // state
    if (g_copper_ready_prog) {
        copper_csw();
        return 0;
    }


    // Jump to the field dependent Copper program if we are in interlace mode.
    // Nothing to do if we are in non-interlaced mode. Note that edits are
    // applied at the time of the odd field to ensure that we don't change
    // things in the "middle" of a frame.
    if (g_copper_is_running_interlaced) {
        CHIPSET_BASE_DECL(cp);
        const uint16_t isLongFrame = *CHIPSET_REG_16(cp, VPOSR) & 0x8000;

        if (isLongFrame && g_copper_running_prog->ed.pending) {
            copper_prog_apply_edits(g_copper_running_prog, g_copper_running_prog->odd_entry);
            copper_prog_apply_edits(g_copper_running_prog, g_copper_running_prog->even_entry);
            doClearEdits = true;
        }

        *CHIPSET_REG_32(cp, COP1LC) = (uint32_t)((isLongFrame) ? g_copper_running_prog->odd_entry : g_copper_running_prog->even_entry);
        *CHIPSET_REG_16(cp, COPJMP1) = 0;
    }
    else if (g_copper_running_prog->ed.pending) {
        copper_prog_apply_edits(g_copper_running_prog, g_copper_running_prog->odd_entry);
        doClearEdits = true;
    }


    if (doClearEdits) {
        copper_prog_clear_edits(g_copper_running_prog);
    }

    return 0;
}

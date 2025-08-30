//
//  amiga/copper.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/25/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "copper.h"
#include <kern/kalloc.h>
#include <kern/kernlib.h>
#include <machine/irq.h>
#include <machine/amiga/chipset.h>
#include <sched/sem.h>

extern int copper_irq(void);


// Must be > 0
#define MAX_RETIRED_PROGS   2
static irq_handler_t            g_copper_vblank;
static copper_prog_t _Nonnull   g_copper_null_prog;
static copper_prog_t _Nullable  g_copper_ready_prog;
static copper_prog_t _Nonnull   g_copper_running_prog;
static copper_prog_t _Nullable  g_copper_retired_progs;
static sem_t                    g_copper_notify_sem;
static int8_t                   g_copper_is_running_interlaced;


// Frees the given Copper program.
static void copper_prog_destroy(copper_prog_t _Nullable prog)
{
    if (prog) {
        kfree(prog->prog);
        prog->prog = NULL;
        prog->even_entry = NULL;
        prog->odd_entry = NULL;
    }
    kfree(prog);
}

errno_t copper_prog_create(size_t instr_count, copper_prog_t _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    copper_prog_t prog = NULL;

    if (instr_count == 0) {
        return EINVAL;
    }

    const unsigned sim = irq_set_mask(IRQ_MASK_VBLANK);
    copper_prog_t cp = g_copper_retired_progs;
    copper_prog_t pp = NULL;
    copper_prog_t gc_head = NULL;
    int retired_count = 0;

    // Find a retired program that is big enough to hold 'instr_count' instructions 
    while (cp) {
        if (cp->prog_size >= instr_count) {
            if (pp) {
                pp->next = cp->next;
            }
            else {
                g_copper_retired_progs = cp->next;
            }

            prog = cp;
            prog->next = NULL;
            break;
        }
        pp = cp;
        cp = cp->next;
    }


    // We keep at most MAX_RETIRED_PROG and we'll destroy the rest
    cp = g_copper_retired_progs;
    pp = NULL;
    while (cp) {
        if (retired_count >= MAX_RETIRED_PROGS) {
            gc_head = cp;
            pp->next = NULL;
            break;
        }
        pp = cp;
        cp = cp->next;
        retired_count++;
    }
    irq_set_mask(sim);


    // Allocate a new program if we aren't able to reuse a retired program
    if (prog == NULL) {
        err = kalloc_cleared(sizeof(struct copper_prog), (void**)&prog);
        if (err != EOK) {
            return err;
        }

        err = kalloc_options(sizeof(copper_instr_t) * instr_count, KALLOC_OPTION_UNIFIED, (void**)&prog->prog);
        if (err != EOK) {
            kfree(prog);
            return err;
        }

        prog->prog_size = instr_count;
    }


    // Destroy all excess retired programs
    while (gc_head) {
        copper_prog_t np = gc_head->next;

        copper_prog_destroy(gc_head);
        gc_head = np;
    }


    // Prepare the program state
    prog->state = COP_STATE_IDLE;
    prog->odd_entry = prog->prog;
    prog->even_entry = NULL;


    *pOutProg = prog;
    return EOK;
}

// Compiles a Copper program to display the null screen. The null screen shows
// nothing.
static errno_t create_null_copper_prog(uint16_t* _Nonnull nullSpriteData, copper_prog_t _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    copper_prog_t prog = NULL;
    const size_t instrCount = 
              1                     // DMA (OFF)
            + 1                     // CLUT
            + 3                     // BPLCON0, BPLCON1, BPLCON2
            + 2 * SPRITE_COUNT      // SPRxDATy
            + 2                     // DIWSTART, DIWSTOP
            + 2                     // DDFSTART, DDFSTOP
            + 1                     // DMACON (ON)
            + 2;                    // COP_END

    err = copper_prog_create(instrCount, &prog);
    if (err != EOK) {
        return err;
    }

    copper_instr_t* ip = prog->prog;

    // DMACON (OFF)
    *ip++ = COP_MOVE(DMACON, DMACONF_BPLEN | DMACONF_SPREN);


    // CLUT
    *ip++ = COP_MOVE(COLOR00, 0x0fff);


    // BPLCONx
    *ip++ = COP_MOVE(BPLCON0, BPLCON0F_COLOR);
    *ip++ = COP_MOVE(BPLCON1, 0);
    *ip++ = COP_MOVE(BPLCON2, 0);


    // SPRxDATy
    const uint32_t sprpt = (uint32_t)nullSpriteData;
    for (int i = 0, r = SPRITE_BASE; i < SPRITE_COUNT; i++, r += 4) {
        *ip++ = COP_MOVE(r + 0, (sprpt >> 16) & UINT16_MAX);
        *ip++ = COP_MOVE(r + 2, sprpt & UINT16_MAX);
    }


    // DIWSTART / DIWSTOP
    *ip++ = COP_MOVE(DIWSTART, (DIW_NTSC_VSTART << 8) | DIW_NTSC_HSTART);
    *ip++ = COP_MOVE(DIWSTOP, (DIW_NTSC_VSTOP << 8) | DIW_NTSC_HSTOP);


    // DDFSTART / DDFSTOP
    *ip++ = COP_MOVE(DDFSTART, 0x0038);
    *ip++ = COP_MOVE(DDFSTOP, 0x00d0);


    // DMACON
    *ip++ = COP_MOVE(DMACON, DMACONF_SETCLR | DMACONF_SPREN | DMACONF_DMAEN);


    // end instruction
    *ip = COP_END();
    
    *pOutProg = prog;
    return EOK;
}

static void _copper_prog_retire(copper_prog_t _Nonnull prog)
{
    if (prog != g_copper_null_prog) {
        if (g_copper_retired_progs) {
            prog->next = g_copper_retired_progs;
            g_copper_retired_progs = prog;
        }
        else {
            g_copper_retired_progs = prog;
            prog->next = NULL;
        }
    }

    prog->state = COP_STATE_RETIRED;
}


errno_t copper_init(uint16_t* _Nonnull nullSpriteData)
{
    decl_try_err();

    sem_init(&g_copper_notify_sem, 0);
    try(create_null_copper_prog(nullSpriteData, &g_copper_null_prog));

    // Do a forced schedule of the null program
    g_copper_ready_prog = NULL;
    g_copper_running_prog = g_copper_null_prog;
    g_copper_running_prog->state = COP_STATE_RUNNING;
    g_copper_is_running_interlaced = 0;

catch:
    return err;
}

static void wait_vbl(void)
{
    CHIPSET_BASE_DECL(cp);

    for (;;) {
        const uint32_t vposr = (*CHIPSET_REG_32(cp, VPOSR) & 0x1ff00) >> 8;

        if (vposr == 303) {
            break;
        }
    }
}

void copper_start(void)
{
    // Let the Copper run our null program
    CHIPSET_BASE_DECL(cp);

    wait_vbl();
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

void copper_schedule(copper_prog_t _Nullable prog, unsigned flags)
{
    copper_prog_t real_prog = (prog) ? prog : g_copper_null_prog;
    const unsigned sim = irq_set_mask(IRQ_MASK_VBLANK);
    if (g_copper_ready_prog) {
        _copper_prog_retire(g_copper_ready_prog);
    }

    g_copper_ready_prog = real_prog;
    real_prog->state = COP_STATE_READY;
    irq_set_mask(sim);


    if ((flags & COPFLAG_WAIT_RUNNING) == COPFLAG_WAIT_RUNNING) {
        while (real_prog->state == COP_STATE_READY) {
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


    sem_relinquish_irq(&g_copper_notify_sem);
}

// Called at the vertical blank interrupt. Triggers the execution of the correct
// Copper program (odd or even field as needed). Also makes a scheduled program
// active / running if needed.
int copper_irq(void)
{
    // Check whether a new program is scheduled to run. If so move it to running
    // state
    if (g_copper_ready_prog) {
        copper_csw();
        return 0;
    }

    
    // Jump to the field dependent Copper program if we are in interlace mode.
    // Nothing to do if we are in non-interlaced mode
    if (g_copper_is_running_interlaced) {
        CHIPSET_BASE_DECL(cp);
        const uint16_t isLongFrame = *CHIPSET_REG_16(cp, VPOSR) & 0x8000;

        *CHIPSET_REG_32(cp, COP1LC) = (uint32_t)((isLongFrame) ? g_copper_running_prog->odd_entry : g_copper_running_prog->even_entry);
        *CHIPSET_REG_16(cp, COPJMP1) = 0;
    }

    return 0;
}

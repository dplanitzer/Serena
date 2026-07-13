//
//  gd_copper.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd_priv.h"
#include <driver/IOLib.h>
#include <hal/irq.h>
#include <hal/hw/m68k-amiga/chipset.h>
#include <kern/kernlib.h>
#include <sched/sem.h>


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Copper Context Switcher
////////////////////////////////////////////////////////////////////////////////

// Note that copper_asm.s depends on this definition
typedef struct sprite_ctl_change {
    uint32_t* _Nullable ptr;
    uint32_t            ctl;
} sprite_ctl_change_t;


extern void vbl_irq(void);


static irq_handler_t            g_copper_vblank;
copper_prog_t _Nullable         g_copper_ready_prog;        // xref copper_asm.s
copper_prog_t _Nonnull          g_copper_running_prog;
static copper_prog_t _Nullable  g_copper_retired_progs;
static sem_t                    g_copper_notify_sem;
static int                      g_retire_signo;
static vcpu_t _Nullable         g_retire_vcpu;
static int8_t                   g_copper_is_running_interlaced;

volatile uint8_t                g_sprite_change_pending;                // xref copper_asm.s
sprite_ctl_change_t             g_sprite_change_table[SPRITE_COUNT];    // xref copper_asm.s


static errno_t copper_init(copper_prog_t _Nonnull prog, int signo, vcpu_t _Nullable sigvp)
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

static void copper_start(void)
{
    // Let the Copper run our null program
    hw_chips->dmacon = DMACONF_COPEN | DMACONF_SPREN | DMACONF_BPLEN;
    chipset_wait_bof();
    hw_chips->cop1lc = g_copper_running_prog->odd_entry;
    hw_chips->copjmp1 = 0;
    hw_chips->dmacon = DMACONF_SETCLR | DMACONF_COPEN | DMACONF_DMAEN;


    // Activate the Copper context switcher
    g_copper_vblank.id = IRQ_ID_VBLANK;
    g_copper_vblank.priority = IRQ_PRI_HIGHEST + 4;
    g_copper_vblank.enabled = true;
    g_copper_vblank.func = (irq_handler_func_t)vbl_irq;
    g_copper_vblank.arg = NULL;

    irq_add_handler(&g_copper_vblank);
    irq_enable_src(IRQ_ID_VBLANK);
}

static copper_prog_t _Nullable copper_acquire_retired_prog(void)
{
    const unsigned sim = irq_set_mask(IRQ_MASK_VBLANK);
    copper_prog_t prog = g_copper_retired_progs;

    if (prog) {
        g_copper_retired_progs = prog->next;
        prog->next = NULL;
    }

    irq_restore_mask(sim);

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
    irq_restore_mask(sim);


    if ((flags & COPFLAG_WAIT_RUNNING) == COPFLAG_WAIT_RUNNING) {
        while (prog->state == COP_STATE_READY) {
            sem_timedwait(&g_copper_notify_sem, &NANOTIME_INF);
        }
    }
}

#if 0
// Replaced with assembler version 9/16/2025 (copper_asm.s)
copper_prog_t _Nullable copper_unschedule(void)
{
    const unsigned sim = irq_set_mask(IRQ_MASK_VBLANK);
    copper_prog_t prog = g_copper_ready_prog;
    g_copper_ready_prog = NULL;
    irq_restore_mask(sim);
    return prog;
}
#endif

// Called when the Copper scheduler has received a request to switch to a new
// Copper program. Updates the running program, retires the old program, updates
// the Copper state and triggers the first run of the Copper program
static void copper_csw(void)
{
    hw_chips->dmacon = DMACONF_COPEN | DMACONF_BPLEN | DMACONF_SPREN;


    // Retire the currently running program
    _copper_prog_retire(g_copper_running_prog);


    // Move the scheduled program to running state. But be sure to first
    // turn off the Copper and raster DMA. Then move the data. Then turn the
    // Copper DMA back on if we have a prog. The program is responsible for 
    // turning the raster DMA on.
    g_copper_running_prog = g_copper_ready_prog;
    g_copper_running_prog->state = COP_STATE_RUNNING;
    g_copper_ready_prog = NULL;
    register copper_prog_t prog = g_copper_running_prog;


    // Interlaced if we got an odd & even field program
    g_copper_is_running_interlaced = (prog->even_entry != NULL);


    // Install the correct program in the Copper, re-enable DMA and trigger
    // a jump to the program
    if (g_copper_is_running_interlaced) {
        // Handle interlaced (dual field) programs. Which program to activate depends
        // on whether the current field is the even or the odd one
        const uint16_t isLongFrame = hw_chips->vposr & 0x8000;

        hw_chips->cop1lc = (isLongFrame) ? prog->odd_entry : prog->even_entry;
    } else {
        hw_chips->cop1lc = prog->odd_entry;
    }

    hw_chips->copjmp1 = 0;
    hw_chips->dmacon = DMACONF_SETCLR | DMACONF_COPEN | DMACONF_DMAEN;


    if (g_retire_vcpu) {
        vcpu_send_signal(g_retire_vcpu, g_retire_signo);
    }
    sem_post(&g_copper_notify_sem);
}

// Called at the vertical blank interrupt. Triggers the execution of the correct
// Copper program (odd or even field as needed). Also makes a scheduled program
// active / running if needed.
void vbl_irq(void)
{
    register const uint8_t pending_ctls = g_sprite_change_pending;

    // Update scheduled sprite control word updates
    if (pending_ctls) {
        for (register uint8_t i = 0; i < SPRITE_COUNT; i++) {
            if ((pending_ctls & (1 << i)) != 0) {
                g_sprite_change_table[i].ptr[0] = g_sprite_change_table[i].ctl;
            }
        }
        g_sprite_change_pending = 0;
    }


    // Check whether a new program is scheduled to run. If so move it to running
    // state
    if (g_copper_ready_prog) {
        copper_csw();
        return;
    }


    // Jump to the field dependent Copper program if we are in interlace mode.
    // Nothing to do if we are in non-interlaced mode. Note that edits are
    // applied at the time of the odd field to ensure that we don't change
    // things in the "middle" of a frame.
    if (g_copper_is_running_interlaced) {
        const uint16_t isLongFrame = hw_chips->vposr & 0x8000;

        hw_chips->cop1lc = (isLongFrame) ? g_copper_running_prog->odd_entry : g_copper_running_prog->even_entry;
        hw_chips->copjmp1 = 0;
    }
}

#if 0
// Replaced with assembler version 9/16/2025 (copper_asm.s)
void sprite_ctl_submit(int spridx, void* _Nonnull sprptr, uint32_t ctl)
{
    unsigned sim = irq_set_mask(IRQ_MASK_VBLANK);
    g_sprite_change_table[spridx].ptr = sprptr;
    g_sprite_change_table[spridx].ctl = ctl;
    g_sprite_change_pending |= (1 << spridx);
    irq_restore_mask(sim);
}


void sprite_ctl_cancel(int spridx)
{
    unsigned sim = irq_set_mask(IRQ_MASK_VBLANK);
    g_sprite_change_pending &= ~(1 << spridx);
    irq_restore_mask(sim);
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Copper Manager
////////////////////////////////////////////////////////////////////////////////

// Signal sent by the Copper scheduler when a new Copper program has started
// running and the previously running one has been retired
#define SIG_COPRUN  SIG_USER_1

static copper_prog_t _Nullable     g_copprg_cache;
static size_t                      g_copprg_cache_count;
static vcpu_t _Nonnull             g_copper_vp;
static struct waitqueue            g_copper_wq;
static sigset_t                    g_copper_sigs;


static errno_t _create_copper_prog(size_t instr_count, copper_prog_t _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    copper_prog_t prog = NULL;
    copper_prog_t cp = g_copprg_cache;
    copper_prog_t pp = NULL;

    // Find a retired program that is big enough to hold 'instr_count' instructions 
    while (cp) {
        if (cp->prog_size >= instr_count) {
            if (pp) {
                pp->next = cp->next;
            }
            else {
                g_copprg_cache = cp->next;
            }

            prog = cp;
            prog->next = NULL;
            g_copprg_cache_count--;
            break;
        }
        pp = cp;
        cp = cp->next;
    }


    // Allocate a new program if we aren't able to reuse a retired program
    if (prog == NULL) {
        err = copper_prog_create(instr_count, &prog);
        if (err != EOK) {
            return err;
        }
    }


    // Prepare the program state
    prog->state = COP_STATE_IDLE;
    prog->odd_entry = prog->prog;
    prog->even_entry = NULL;


    *pOutProg = prog;
    return EOK;
}

static void _cache_copper_prog(copper_prog_t _Nonnull prog)
{
    clut_t* clut = prog->res.clut;
    Surface* fb = prog->res.fb;

    // User can not delete the CLUT as long as the Copper program was using it.
    // Just remove the reference here
    prog->res.clut = NULL;
    
    Surface_DelRef(fb);
    prog->res.fb = NULL;

    for (int i = 0; i < SPRITE_COUNT; i++) {
        Surface_DelRef(prog->res.spr[i]);
        prog->res.spr[i] = NULL;
    }


    if (g_copprg_cache_count >= MAX_CACHED_COPPER_PROGS) {
        copper_prog_destroy(prog);
        return;
    }

    if (g_copprg_cache) {
        prog->next = g_copprg_cache;
        g_copprg_cache = prog;
    }
    else {
        prog->next = NULL;
        g_copprg_cache = prog;
    }
    g_copprg_cache_count++;
}

copper_prog_t _Nullable copper_get_editable_prog(void)
{
    copper_prog_t prog = copper_unschedule();

    if (prog == NULL) {
        copper_prog_t run_prog = g_copper_running_prog;
        const errno_t err = _create_copper_prog(run_prog->prog_size, &prog);

        if (err != EOK) {
            // should not happen in actual practice because there should always
            // be at least one prog cached.
            return NULL;
        }

        // Accessing the running Copper program without masking irqs is safe here
        // because:
        // * we hold the io_mtx and thus noone else can schedule a new Copper prog
        // * we just took the ready program out and thus the running program
        //   won't get replaced behind our back
        for (size_t i = 0; i < run_prog->prog_size; i++) {
            prog->prog[i] = run_prog->prog[i];
        }

        if (run_prog->even_entry) {
            prog->even_entry = &prog->prog[prog->prog_size / 2];
        }

        prog->loc = run_prog->loc;
        prog->video_conf = run_prog->video_conf;
        prog->res = run_prog->res;

        if (prog->res.fb) {
            Surface_AddRef(prog->res.fb);
        }

        for (int i = 0; i < SPRITE_COUNT; i++) {
            Surface_AddRef(prog->res.spr[i]);
        }
    }
    return prog;
}


static void copper_manager(void* ignore)
{
    gdLock();

    for (;;) {
        copper_prog_t prog;
        int signo;
        bool hasChange = false;

        while ((prog = copper_acquire_retired_prog()) != NULL) {
            _cache_copper_prog(prog);
            hasChange = true;
        }


        if (hasChange && g_screen_conf_observer) {
            vcpu_send_signal(g_screen_conf_observer, g_screen_conf_signal);
        }


        gdUnlock();
        vcpu_sigwait(&g_copper_wq, &g_copper_sigs, 0, TICKS_MAX, &signo);
        gdLock();
    }

    gdUnlock();
}

errno_t _gdInitCopper(void)
{
    decl_try_err();
    copper_prog_t nullCopperProg;

    wq_init(&g_copper_wq);
    g_copper_sigs = sig_bit(SIG_COPRUN);

    try(create_null_copper_prog(&nullCopperProg));
    try(IOAcquireVirtualProcessor((vcpu_func_t)copper_manager, NULL, VCPU_QOS_URGENT, VCPU_PRI_NORMAL, &g_copper_vp));

    
    // Initialize the Copper scheduler
    copper_init(nullCopperProg, SIG_COPRUN, g_copper_vp);
    copper_start();

    IOResumeVirtualProcessor(g_copper_vp);

catch:
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Copper Program Generators
////////////////////////////////////////////////////////////////////////////////

errno_t create_null_copper_prog(copper_prog_t _Nullable * _Nonnull pOutProg)
{
    return create_screen_copper_prog(get_null_video_conf(), NULL, NULL, pOutProg);
}

errno_t create_screen_copper_prog(const video_conf_t* _Nonnull vc, Surface* _Nullable fb, clut_t* _Nullable clut, copper_prog_t _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    const size_t instrCount = calc_copper_prog_instruction_count(vc);
    copper_prog_t prog = NULL;

    err = _create_copper_prog(instrCount, &prog);
    if (err == EOK) {
        copper_prog_compile(prog, vc, fb, clut, g_sprite, g_null_sprite_surface, g_light_pen_enabled);
    }

    *pOutProg = prog;

    return err;
}

//
//  driver/hw/m68k-amiga/irq.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <hal/irq.h>
#include <hal/hw/m68k-amiga/chipset.h>
#include <kern/assert.h>
#include <kern/kernlib.h>


static void _nop_irq_handler(void)
{
    // do nothing
}


irq_clock_func_t    g_irq_clock_func = (irq_clock_func_t)_nop_irq_handler;
void* _Nullable     g_irq_clock_arg;

irq_key_func_t      g_irq_key_func = (irq_key_func_t)_nop_irq_handler;
void* _Nullable     g_irq_key_arg;

irq_direct_func_t   g_irq_disk_block_func = (irq_direct_func_t)_nop_irq_handler;
void* _Nullable     g_irq_disk_block_arg;

size_t  g_irq_stat_uninit;
size_t  g_irq_stat_spurious;
size_t  g_irq_stat_nmi;


size_t irq_get_stat(int stat_id)
{
    switch (stat_id) {
        case IRQ_STAT_UNINITIALIZED_COUNT:
            return g_irq_stat_uninit;

        case IRQ_STAT_SPURIOUS_COUNT:
            return g_irq_stat_spurious;

        case IRQ_STAT_NON_MASKABLE_COUNT:
            return g_irq_stat_nmi;

        default:
            return 0;
    }
}


// irq_handlers_for_id() is defined in the platform specific irq.c file
void irq_add_handler(irq_handler_t* _Nonnull h)
{
    const unsigned sim = irq_set_mask(IRQ_MASK_ALL);

    irq_handler_t** listp = irq_handlers_for_id(h->id);
    register irq_handler_t* ph = NULL;
    register irq_handler_t* ch = *listp;

    while (ch) {
        if (ch->priority > h->priority) {
            break;
        }
        
        ph = ch;
        ch = ch->next;
    }
    
    if (ph) {
        h->next = ph->next;
        ph->next = h;
    }
    else {
        h->next = *listp;
        *listp = h;
    }

    irq_restore_mask(sim);
}

void irq_remove_handler(irq_handler_t* _Nullable h)
{
    const unsigned sim = irq_set_mask(IRQ_MASK_ALL);

    irq_handler_t** listp = irq_handlers_for_id(h->id);
    irq_handler_t* ph = NULL;
    irq_handler_t* ch = *listp;
    bool found = false;

    while (ch) {
        if (ch == h) {
            found = true;
            break;
        }

        ph = ch;
        ch = ch->next;
    }

    if (found) {
        if (ph) {
            ph->next = h->next;
        }
        else {
            *listp = h->next;
        }
        h->next = NULL;
    }

    irq_restore_mask(sim);
}

void irq_set_handler_enabled(irq_handler_t* _Nonnull h, bool enabled)
{
    const unsigned sim = irq_set_mask(IRQ_MASK_ALL);

    h->enabled = enabled;
    irq_restore_mask(sim);
}

// Called from the IRQ context. Run all handlers for the given interrupt list
void _irq_run_handlers(irq_handler_t* _Nullable irq_list)
{
    register irq_handler_t* ch = irq_list;

    while (ch) {
        if (ch->func && ch->enabled) {
            if (ch->func(ch->arg)) {
                break;
            }
        }
        ch = ch->next;
    }
}


void irq_set_direct_handler(int irq_id, irq_direct_func_t _Nonnull f, void* _Nullable arg)
{
    const unsigned sim = irq_set_mask(IRQ_MASK_ALL);

    switch (irq_id) {
        case IRQ_ID_DISK_BLOCK:
            g_irq_disk_block_func = f;
            g_irq_disk_block_arg = arg;
            break;

        case IRQ_ID_MONOTONIC_CLOCK:
            g_irq_clock_func = (irq_clock_func_t)f;
            g_irq_clock_arg = arg;
            break;

        case IRQ_ID_KEYBOARD:
            g_irq_key_func = (irq_key_func_t)f;
            g_irq_key_arg = arg;
            break;

        default:
            abort();
            break;
    }
    irq_restore_mask(sim);
}


irq_handler_t* _Nullable    g_vbl_handlers;
irq_handler_t* _Nullable    g_int2_handlers;
irq_handler_t* _Nullable    g_int6_handlers;

irq_handler_t** irq_handlers_for_id(int irq_id)
{
    switch (irq_id) {
        case IRQ_ID_VBLANK:     return &g_vbl_handlers;
        case IRQ_ID_INT2_EXTERN:        return &g_int2_handlers;
        case IRQ_ID_INT6_EXTERN:        return &g_int6_handlers;
        default:                        return NULL;
    }
}

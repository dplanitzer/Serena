//
//  amiga/irq.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kern/assert.h>
#include <kern/kernlib.h>
#include <machine/irq.h>
#include <machine/amiga/chipset.h>


irq_clock_func_t    g_irq_clock_func;
void* _Nullable     g_irq_clock_arg;

irq_key_func_t      g_irq_key_func;
void* _Nullable     g_irq_key_arg;

irq_direct_func_t   g_irq_disk_block_func;
void* _Nullable     g_irq_disk_block_arg;


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
    irq_set_mask(sim);
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

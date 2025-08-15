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
    const int is = irq_disable();

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
    irq_restore(is);
}

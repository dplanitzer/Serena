//
//  irq.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kern/kernlib.h>
#include <machine/irq.h>


size_t  g_irq_stat_uninit;
size_t  g_irq_stat_spurious;
size_t  g_irq_stat_nmi;


size_t irq_get_stat(int stat_id)
{
    size_t r;
    const int is = irq_disable();

    switch (stat_id) {
        case IRQ_STAT_UNINITIALIZED_COUNT:
            r = g_irq_stat_uninit;
            break;

        case IRQ_STAT_SPURIOUS_COUNT:
            r = g_irq_stat_spurious;
            break;

        case IRQ_STAT_NON_MASKABLE_COUNT:
            r = g_irq_stat_nmi;
            break;

        default:
            r = 0;
            break;
    }
    irq_restore(is);

    return r;
}

//
//  irq.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <machine/irq.h>
#include <machine/amiga/chipset.h>

irq_clock_func  g_irq_clock_func;
void* _Nullable g_irq_clock_arg;

void irq_set_clock_func(irq_clock_func _Nonnull f, void* _Nullable arg)
{
    const int is = irq_disable();
    g_irq_clock_func = f;
    g_irq_clock_arg = arg;
    irq_restore(is);
}


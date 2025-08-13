//
//  irq.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kern/kernlib.h>
#include <machine/irq.h>
#include <machine/amiga/chipset.h>

irq_clock_func_t    g_irq_clock_func;
void* _Nullable     g_irq_clock_arg;

irq_key_func_t      g_irq_key_func;
void* _Nullable     g_irq_key_arg;


void irq_set_clock_func(irq_clock_func_t _Nonnull f, void* _Nullable arg)
{
    const int is = irq_disable();
    g_irq_clock_func = f;
    g_irq_clock_arg = arg;
    irq_restore(is);
}

void irq_set_key_func(irq_key_func_t _Nonnull f, void* _Nullable arg)
{
    const int is = irq_disable();
    g_irq_key_func = f;
    g_irq_key_arg = arg;
    irq_restore(is);
}

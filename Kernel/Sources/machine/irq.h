//
//  irq.h
//  Kernel
//
//  Created by Dietmar Planitzer on 8/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _IRQ_H
#define _IRQ_H

#include <kern/types.h>
#ifdef MACHINE_AMIGA
#include <machine/amiga/irq.h>
#else
#error "unknown platform"
#endif

// Enables interrupt handling.
extern void irq_enable(void);

// Disables interrupt handling and returns the previous interrupt handling state.
// Returns the old IRQ state
extern int irq_disable(void);

// Restores the given interrupt handling state.
extern void irq_restore(int state);


// Enables generation of the given interrupt type. This implicitly turns on the
// master IRQ switch.
// Note that we always turn on the master IRQ switch, external IRQs and the ports
// IRQs because it makes things simpler. External & ports IRQs are masked in the
// chips that are the source of an IRQ. CIA A/B chips.
extern void irq_enable_src(int interruptId);

// Disables the generation of the given interrupt type. This function leaves the
// master IRQ switch enabled. It doesn't matter 'cause we turn the IRQ source off
// anyway.
extern void irq_disable_src(int interruptId);

#endif /* _IRQ_H */

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

extern void irq_enable(void);
extern int irq_disable(void);
extern void irq_restore(int state);

#endif /* _IRQ_H */

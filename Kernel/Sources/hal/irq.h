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
#include <hal/hw/m68k-amiga/irq.h>
#else
#error "unknown platform"
#endif


// Sets the CPU's interrupt priority mask to 'mask' and returns the previous mask.
// Calls to irq_set_mask() may be nested by pairing them with irq_restore_mask():
// first call sets the new mask and returns the previous mask and the second call
// in a pair restores the previously saved mask. This function only establishes
// the new mask if it is more restrictive than the currently active mask. It
// returns the currently active mask if any case. 
extern unsigned irq_set_mask(unsigned mask);

// Restores the CPU IRQ mask to 'mask'.
extern void irq_restore_mask(unsigned mask);


// Enables generation of the given interrupt type. This implicitly turns on the
// master IRQ switch.
// Note that we always turn on the master IRQ switch, external IRQs and the ports
// IRQs because it makes things simpler. External & ports IRQs are masked in the
// chips that are the source of an IRQ. CIA A/B chips.
extern void irq_enable_src(int irq_id);

// Disables the generation of the given interrupt type. This function leaves the
// master IRQ switch enabled. It doesn't matter 'cause we turn the IRQ source off
// anyway.
extern void irq_disable_src(int irq_id);


// Sets a function that should be called when an interrupt of type 'irq_id' is
// triggered. The function will receive 'arg' as its first argument.
// @Note: the function will run in the interrupt context
typedef void (*irq_direct_func_t)(void* _Nullable arg);
extern void irq_set_direct_handler(int irq_id, irq_direct_func_t _Nonnull f, void* _Nullable arg);


// Returns 0 to continue IRQ processing and a value != 0 to end IRQ processing
typedef int (*irq_handler_func_t)(void* _Nullable arg);

#define IRQ_PRI_HIGHEST -128
#define IRQ_PRI_NORMAL  0
#define IRQ_PRI_LOWEST  127

typedef struct irq_handler {
    struct irq_handler* _Nullable   next;
    irq_handler_func_t _Nonnull     func;
    void* _Nullable                 arg;
    int8_t                          id;     // IRQ_ID_XXX
    int8_t                          priority;
    bool                            enabled;
    int8_t                          reserved;
} irq_handler_t;

extern void irq_add_handler(irq_handler_t* _Nonnull h);
extern void irq_remove_handler(irq_handler_t* _Nullable h);

extern void irq_set_handler_enabled(irq_handler_t* _Nonnull h, bool enabled);


// Returns the requested irq related statistics.
#define IRQ_STAT_UNINITIALIZED_COUNT    0
#define IRQ_STAT_SPURIOUS_COUNT         1
#define IRQ_STAT_NON_MASKABLE_COUNT     2

extern size_t irq_get_stat(int stat_id);


//
// HAL Internals
//

// Returns the list of IRQ handlers for the given 'irq_id'.
// @Platform: required
extern irq_handler_t** irq_handlers_for_id(int irq_id);

#endif /* _IRQ_H */

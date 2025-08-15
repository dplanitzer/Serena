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


// irq_handlers_for_id() is defined in the platform specific irq.c file
void irq_add_handler(irq_handler_t* _Nonnull h)
{
    const int is = irq_disable();

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

    irq_restore(is);
}

void irq_remove_handler(irq_handler_t* _Nullable h)
{
    const int is = irq_disable();

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

    irq_restore(is);
}

void irq_set_handler_enabled(irq_handler_t* _Nonnull h, bool enabled)
{
    const int is = irq_disable();

    h->enabled = enabled;
    irq_restore(is);
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

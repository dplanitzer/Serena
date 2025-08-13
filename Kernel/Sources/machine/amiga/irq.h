//
//  amiga/irq.h
//  Kernel
//
//  Created by Dietmar Planitzer on 8/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _AMIGA_IRQ_H
#define _AMIGA_IRQ_H

#include <machine/cpu.h>

// Supported interrupts
// http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0142.html
// http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0036.html
#define IRQ_ID_CIA_B_FLAG                     23
#define IRQ_ID_CIA_B_SP                       22
#define IRQ_ID_CIA_B_ALARM                    21
#define IRQ_ID_CIA_B_TIMER_B                  20
#define IRQ_ID_CIA_B_TIMER_A                  19

#define IRQ_ID_CIA_A_FLAG                     18
#define IRQ_ID_CIA_A_SP                       17
#define IRQ_ID_CIA_A_ALARM                    16
#define IRQ_ID_CIA_A_TIMER_B                  15    // Direct Hook: monotonic clock + scheduler
#define IRQ_ID_CIA_A_TIMER_A                  14

#define IRQ_ID_EXTERN                         13
#define IRQ_ID_DISK_SYNC                      12
#define IRQ_ID_SERIAL_RECEIVE_BUFFER_FULL     11
#define IRQ_ID_AUDIO3                         10
#define IRQ_ID_AUDIO2                         9
#define IRQ_ID_AUDIO1                         8
#define IRQ_ID_AUDIO0                         7
#define IRQ_ID_BLITTER                        6
#define IRQ_ID_VERTICAL_BLANK                 5
#define IRQ_ID_COPPER                         4
#define IRQ_ID_PORTS                          3
#define IRQ_ID_SOFT                           2
#define IRQ_ID_DISK_BLOCK                     1
#define IRQ_ID_SERIAL_TRANSMIT_BUFFER_EMPTY   0

#define IRQ_ID_COUNT                          24


typedef void (*irq_clock_func_t)(void* _Nullable arg, excpt_frame_t* _Nonnull efp);
typedef void (*irq_key_func_t)(void* _Nullable arg, int key);

extern void irq_set_clock_func(irq_clock_func_t _Nonnull f, void* _Nullable arg);
extern void irq_set_key_func(irq_key_func_t _Nonnull f, void* _Nullable arg);

#endif /* _AMIGA_IRQ_H */

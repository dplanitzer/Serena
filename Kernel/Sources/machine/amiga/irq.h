//
//  amiga/irq.h
//  Kernel
//
//  Created by Dietmar Planitzer on 8/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _AMIGA_IRQ_H
#define _AMIGA_IRQ_H

#include <kern/types.h>


// Supported interrupts
// http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0142.html
// http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0036.html
#define INTERRUPT_ID_CIA_B_FLAG                     23
#define INTERRUPT_ID_CIA_B_SP                       22
#define INTERRUPT_ID_CIA_B_ALARM                    21
#define INTERRUPT_ID_CIA_B_TIMER_B                  20
#define INTERRUPT_ID_CIA_B_TIMER_A                  19

#define INTERRUPT_ID_CIA_A_FLAG                     18
#define INTERRUPT_ID_CIA_A_SP                       17
#define INTERRUPT_ID_CIA_A_ALARM                    16
//#define INTERRUPT_ID_CIA_A_TIMER_B                  15    // Direct Hook: monotonic clock + scheduler
#define INTERRUPT_ID_CIA_A_TIMER_A                  14

#define INTERRUPT_ID_EXTERN                         13
#define INTERRUPT_ID_DISK_SYNC                      12
#define INTERRUPT_ID_SERIAL_RECEIVE_BUFFER_FULL     11
#define INTERRUPT_ID_AUDIO3                         10
#define INTERRUPT_ID_AUDIO2                         9
#define INTERRUPT_ID_AUDIO1                         8
#define INTERRUPT_ID_AUDIO0                         7
#define INTERRUPT_ID_BLITTER                        6
#define INTERRUPT_ID_VERTICAL_BLANK                 5
#define INTERRUPT_ID_COPPER                         4
#define INTERRUPT_ID_PORTS                          3
#define INTERRUPT_ID_SOFT                           2
#define INTERRUPT_ID_DISK_BLOCK                     1
#define INTERRUPT_ID_SERIAL_TRANSMIT_BUFFER_EMPTY   0

#define INTERRUPT_ID_COUNT                          24

#endif /* _AMIGA_IRQ_H */

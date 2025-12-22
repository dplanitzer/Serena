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

// Supported physical interrupts
// http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0142.html
// http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0036.html
#define IRQ_ID_CIA_B_FLAG       23
#define IRQ_ID_CIA_B_SP         22
#define IRQ_ID_CIA_B_ALARM      21
#define IRQ_ID_CIA_B_TIMER_B    20
#define IRQ_ID_CIA_B_TIMER_A    19

#define IRQ_ID_CIA_A_FLAG       18
#define IRQ_ID_CIA_A_SP         17
#define IRQ_ID_CIA_A_ALARM      16
#define IRQ_ID_CIA_A_TIMER_B    15    // Direct Hook: monotonic clock + scheduler
#define IRQ_ID_CIA_A_TIMER_A    14

#define IRQ_ID_EXTERN           13
#define IRQ_ID_DISK_SYNC        12
#define IRQ_ID_SERIAL_RBF       11
#define IRQ_ID_AUDIO3           10
#define IRQ_ID_AUDIO2           9
#define IRQ_ID_AUDIO1           8
#define IRQ_ID_AUDIO0           7
#define IRQ_ID_BLITTER          6
#define IRQ_ID_VBLANK           5
#define IRQ_ID_COPPER           4
#define IRQ_ID_PORTS            3
#define IRQ_ID_SOFT             2
#define IRQ_ID_DISK_BLOCK       1
#define IRQ_ID_SERIAL_TBE       0

#define IRQ_ID_COUNT            24

// Logical interrupt names
#define IRQ_ID_KEYBOARD             IRQ_ID_CIA_A_SP
#define IRQ_ID_MONOTONIC_CLOCK      IRQ_ID_CIA_A_TIMER_B
#define IRQ_ID_INT2_EXTERN          IRQ_ID_PORTS
#define IRQ_ID_INT6_EXTERN          IRQ_ID_EXTERN
#define IRQ_ID_ZORRO_LOW            IRQ_ID_INT2_EXTERN
#define IRQ_ID_ZORRO_HIGH           IRQ_ID_INT6_EXTERN


// Interrupt masks
#define IRQ_MASK_ALL                0x0700
#define IRQ_MASK_CIA_B              0x0600
#define IRQ_MASK_EXTERN             0x0600
#define IRQ_MASK_DISK_SYNC          0x0500
#define IRQ_MASK_SERIAL_RBF         0x0500
#define IRQ_MASK_AUDIO3             0x0400
#define IRQ_MASK_AUDIO2             0x0400
#define IRQ_MASK_AUDIO1             0x0400
#define IRQ_MASK_AUDIO0             0x0400
#define IRQ_MASK_BLITTER            0x0300
#define IRQ_MASK_VBLANK             0x0300
#define IRQ_MASK_COPPER             0x0300
#define IRQ_MASK_CIA_A              0x0200
#define IRQ_MASK_PORTS              0x0200
#define IRQ_MASK_SOFT               0x0100
#define IRQ_MASK_DISK_BLOCK         0x0100
#define IRQ_MASK_SERIAL_TBE         0x0100
#define IRQ_MASK_NONE               0x0000

// Logical interrupt mask names
#define IRQ_MASK_KEYBOARD           IRQ_MASK_CIA_A
#define IRQ_MASK_INT2_EXTERN        IRQ_MASK_PORTS
#define IRQ_MASK_INT6_EXTERN        IRQ_MASK_EXTERN


// Direct handler type definitions for specific interrupt types
typedef void (*irq_clock_func_t)(void* _Nullable arg, excpt_frame_t* _Nonnull efp);
typedef void (*irq_key_func_t)(void* _Nullable arg, int key);

#endif /* _AMIGA_IRQ_H */

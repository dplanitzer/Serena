//
//  delay.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _DELAY_H
#define _DELAY_H 1

#include <kern/errno.h>
#include <kern/types.h>
#include <kpi/signal.h>

struct WaitQueue;


// Note: Interruptable
extern void delay_init(void);

extern void delay_us(useconds_t us);
extern void delay_ms(mseconds_t ms);
extern void delay_sec(time_t sec);

#endif /* _DELAY_H */

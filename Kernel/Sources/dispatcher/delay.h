//
//  delay.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef Delay_h
#define Delay_h

#include <kern/errno.h>
#include <kern/types.h>

extern void delay_init(void);

extern void delay_us(useconds_t us);
extern void delay_ms(mseconds_t ms);
extern void delay_sec(time_t sec);

extern errno_t _sleep(int flags, const struct timespec* _Nonnull wtp, struct timespec* _Nullable rmtp);

#endif /* Delay_h */

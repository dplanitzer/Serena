//
//  m68k-amiga/gary5719.h
//  Kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _GARY5719_H
#define _GARY5719_H

#include <stdint.h>


#define hw_gary ((volatile struct gary5719*)0xde0000u)


struct gary5719 {
    uint8_t timeout;
    uint8_t toenb;
    uint8_t coldstart;
};


// Register Bit
#define GARY_REGF_BIT   0x80

#endif /* _GARY5719_H */

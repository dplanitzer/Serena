//
//  m68k-amiga/ramsey390541.h
//  Kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _RAMSEY390541_H
#define _RAMSEY390541_H

#include <stdbool.h>
#include <stdint.h>


#define hw_ramsey   ((volatile struct ramsey390541*)0xde0000u)


struct ramsey390541 {
    uint8_t pad[3];     // these are actually Gary registers
    uint8_t cr;
    uint8_t pad_40[63];
    uint8_t version;
};


// RAMSEY chip versions (32bit Amigas only. Like A3000 / A4000)
#define RAMSEY_rev4 0x0d
#define RAMSEY_rev7 0x0f


// Control Register
#define RAMSEY_CRF_PAGE_MODE    0x01
#define RAMSEY_CRF_BURST_MODE   0x02
#define RAMSEY_CRF_WRAP         0x04
#define RAMSEY_CRF_RAM_SIZE     0x08
#define RAMSEY_CRF_RAM_WIDTH    0x10
#define RAMSEY_CRF_SKIP         0x20
#define RAMSEY_CRF_REFRESH_RATE 0x60    /* mask(5..6) */
#define RAMSEY_CRF_TEST         0x80

#define RAMSEY_REFRESH_154      0x00    /* 16MHz: 9.24us,  25MHz: 6.16us; Default for 16MHz systems */
#define RAMSEY_REFRESH_238      0x01    /* 16MHz: 14.28us, 25MHz: 9.52us; Default for 25MHz systems */
#define RAMSEY_REFRESH_380      0x02    /* 16MHz: 22.8us,  25MHz: 15.2us */
#define RAMSEY_REFRESH_OFF      0x03


extern uint8_t ramsey_version(void);
extern void ramsey_set_page_mode_enabled(bool flag);
extern void ramsey_enable_page_burst_mode(void);

#endif /* _RAMSEY390541_H */

//
//  m68k-amiga/cia8520.h
//  Kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _CIA8520_H
#define _CIA8520_H

#include <stdint.h>


#define hw_cia_a    ((volatile struct cia8520*)0xbfe001u)
#define hw_cia_b    ((volatile struct cia8520*)0xbfd000u)


struct cia8520 {
    uint8_t pra;
    uint8_t pad_100[255];
    uint8_t prb;
    uint8_t pad_200[255];
    uint8_t ddra;
    uint8_t pad_300[255];
    uint8_t ddrb;
    uint8_t pad_400[255];
    uint8_t talo;
    uint8_t pad_500[255];
    uint8_t tahi;
    uint8_t pad_600[255];
    uint8_t tblo;
    uint8_t pad_700[255];
    uint8_t tbhi;
    uint8_t pad_800[255];
    uint8_t todlo;
    uint8_t pad_900[255];
    uint8_t todmid;
    uint8_t pad_a00[255];
    uint8_t todhi;
    uint8_t pad_b00[255];
    uint8_t na;
    uint8_t pad_c00[255];
    uint8_t sdr;
    uint8_t pad_d00[255];
    uint8_t icr;
    uint8_t pad_e00[255];
    uint8_t cra;
    uint8_t pad_f00[255];
    uint8_t crb;
};


#define CIAA_PRAF_OVL       0x01
#define CIAA_PRAF_LED       0x02
#define CIAA_PRAF_DSKCHNG   0x04
#define CIAA_PRAF_DSKWPRO   0x08
#define CIAA_PRAF_DSKTK0    0x10
#define CIAA_PRAF_DSKRDY    0x20
#define CIAA_PRAF_FIR0      0x40
#define CIAA_PRAF_FIR1      0x80

#define CIAA_PRAB_DSKRDY    5

#define CIAB_PRAF_BUSY  0x01
#define CIAB_PRAF_POUT  0x02
#define CIAB_PRAF_SEL   0x04
#define CIAB_PRAF_DSR   0x08
#define CIAB_PRAF_CTS   0x10
#define CIAB_PRAF_CD    0x20
#define CIAB_PRAF_RTS   0x40
#define CIAB_PRAF_DTR   0x80

#define CIAB_PRBF_DSKSTEP   0x01
#define CIAB_PRBF_DSKDIR    0x02
#define CIAB_PRBF_DSKSIDE   0x04
#define CIAB_PRBF_DSKSEL0   0x08
#define CIAB_PRBF_DSKSEL1   0x10
#define CIAB_PRBF_DSKSEL2   0x20
#define CIAB_PRBF_DSKSEL3   0x40
#define CIAB_PRBF_DSKSELALL (CIAB_PRBF_DSKSEL0 | CIAB_PRBF_DSKSEL1 | CIAB_PRBF_DSKSEL2 | CIAB_PRBF_DSKSEL3)
#define CIAB_PRBF_DSKMTR    0x80

#define CIAB_PRBB_DSKSEL0   3

#define CIA_IRCF_IR         0x80
#define CIA_IRCF_SC         0x80
#define CIA_IRCF_FLG        0x10
#define CIA_IRCF_SP         0x08
#define CIA_IRCF_ALRM       0x04
#define CIA_IRCF_TB         0x02
#define CIA_IRCF_TA         0x01

#define CIA_CRAF_START      0x01
#define CIA_CRAF_PBON       0x02
#define CIA_CRAF_OUTMODE    0x04
#define CIA_CRAF_RUNMODE    0x08
#define CIA_CRAF_LOAD       0x10
#define CIA_CRAF_INMODE     0x20
#define CIA_CRAF_SPMODE     0x40
#define CIA_CRAF_UNUSED     0x80

#define CIA_CRBF_START          0x01
#define CIA_CRBF_PBON           0x02
#define CIA_CRBF_OUTMODE        0x04
#define CIA_CRBF_RUNMODE        0x08
#define CIA_CRBF_LOAD           0x10
#define CIA_CRBF_INMODE_MASK    0x60
#define CIA_CRBF_ALARM          0x80

#endif /* _CIA8520_H */

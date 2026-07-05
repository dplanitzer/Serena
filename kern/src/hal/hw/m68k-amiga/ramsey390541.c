//
//  ramsey390541.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/31/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ramsey390541.h"


uint8_t ramsey_version(void)
{
    const uint8_t v = hw_ramsey->version;

    switch (v) {
        case RAMSEY_rev4:
        case RAMSEY_rev7:
            return v;
            
        default:
            return 0;
    }
}

void ramsey_set_page_mode_enabled(bool flag)
{
    uint8_t ref, r = hw_ramsey->cr;

    if (flag) {
        r |= RAMSEY_CRF_PAGE_MODE;
        ref = 0;
    } else {
        r &= ~RAMSEY_CRF_PAGE_MODE;
        ref = RAMSEY_CRF_PAGE_MODE;
    }
    hw_ramsey->cr = r;


    // Wait for the change to take effect
    do {
        r = hw_ramsey->cr;
    } while ((r & RAMSEY_CRF_PAGE_MODE) == ref);
}

void ramsey_enable_page_burst_mode(void)
{
    // Note that the refresh delay needs to be < 10us. However RAMSEY automatically
    // selects the right refresh mode by default. So we just leave the refresh
    // setting alone.
    uint8_t r = hw_ramsey->cr;

    r |= RAMSEY_CRF_PAGE_MODE;
    r |= RAMSEY_CRF_BURST_MODE;
    r &= ~RAMSEY_CRF_WRAP;  // Needs to be off for the 68040

    hw_ramsey->cr = r;


    // Wait for the change to take effect
    do {
        r = hw_ramsey->cr;
    } while ((r & RAMSEY_CRF_BURST_MODE) == 0);
}

//
//  mfm.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "FloppyDisk.h"


// The MFM decoder/encoder code is based on:
// see http://lclevy.free.fr/adflib/adf_info.html
//
// The following copyright notice applies to the functions:
// mfm_decode_sector(), mfm_encode_sector()
//
//
// This document is Copyright (C) 1997-1999 by Laurent Clévy, but may be freely distributed, provided the author name and addresses are included and no money is charged for this document.
//
// This document is provided "as is". No warranties are made as to its correctness.
//
// Amiga and AmigaDOS are registered Trademarks of Gateway 2000.
// Macintosh is a registered Trademark of Apple.
//


// MFM decodes a sector.
// \param input    MFM coded data buffer (size == 2*data_size)
// \param output    decoded data buffer (size == data_size)
// \param data_size size in long, 1 for header's info, 4 for header's sector label
void mfm_decode_sector(const uint32_t* input, uint32_t* output, int data_size)
{
#define MASK 0x55555555    /* 01010101 ... 01010101 */
    uint32_t odd_bits, even_bits;
    uint32_t chksum = 0;
    
    /* the decoding is made here long by long : with data_size/4 iterations */
    for (int count = 0; count < data_size; count++) {
        odd_bits = *input;                  /* longs with odd bits */
        even_bits = *(input + data_size);   /* longs with even bits : located 'data_size' bytes farther */
        chksum ^= odd_bits;
        chksum ^= even_bits;
        /*
         * MFM decoding, explained on one byte here (o and e will produce t) :
         * the MFM bytes 'abcdefgh' == o and 'ijklmnop' == e will become
         * e & 0x55U = '0j0l0n0p'
         * ( o & 0x55U) << 1 = 'b0d0f0h0'
         * '0j0l0n0p' | 'b0d0f0h0' = 'bjdlfnhp' == t
         */
        *output = (even_bits & MASK) | ((odd_bits & MASK) << 1);
        input++;        /* next 'odd' long and 'even bits' long  */
        output++;       /* next location of the future decoded long */
    }
    chksum &= MASK;    /* must be 0 after decoding */
}


// MFM encodes a sector
void mfm_encode_sector(const uint32_t* input, uint32_t* output, int data_size)
{
    uint32_t* output_odd = output;
    uint32_t* output_even = output + data_size;
    
    for (int count = 0; count < data_size; count++) {
        const uint32_t data = *input;
        uint32_t odd_bits = 0;
        uint32_t even_bits = 0;
        uint32_t prev_odd_bit = 0;
        uint32_t prev_even_bit = 0;
        
        //    user's data bit      MFM coded bits
        //    ---------------      --------------
        //    1                    01
        //    0                    10 if following a 0 data bit
        //    0                    00 if following a 1 data bit
        for (int8_t i_even = 30; i_even >= 0; i_even -= 2) {
            const int8_t i_odd = i_even + 1;
            const uint32_t cur_odd_bit = data & (1 << i_odd);
            const uint32_t cur_even_bit = data & (1 << i_even);
            
            if (cur_odd_bit) {
                odd_bits |= (1 << i_even);
            } else if (!cur_odd_bit && !prev_odd_bit) {
                odd_bits |= (1 << i_odd);
            }
            
            if (cur_even_bit) {
                even_bits |= (1 << i_even);
            } else if (!cur_even_bit && !prev_even_bit) {
                even_bits |= (1 << i_odd);
            }
            
            prev_odd_bit = cur_odd_bit;
            prev_even_bit = cur_even_bit;
        }
        
        *output_odd = odd_bits;
        *output_even = even_bits;
        
        input++;
        output_odd++;
        output_even++;
    }
}

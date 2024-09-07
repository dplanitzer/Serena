//
//  mfm.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "mfm.h"


#define MASK 0x55555555

// MFM decodes a sector.
//
// Based on:
// see http://lclevy.free.fr/adflib/adf_info.html
//
// This function is Copyright (C) 1997-1999 by Laurent Clévy, but may be freely distributed, provided the author name and addresses are included and no money is charged for this document.
//
// This document is provided "as is". No warranties are made as to its correctness.
//
// Amiga and AmigaDOS are registered Trademarks of Gateway 2000.
// Macintosh is a registered Trademark of Apple.
//
// \param input    MFM coded data buffer (size == 2*data_size)
// \param output    decoded data buffer (size == data_size)
// \param data_size size in long, 1 for header's info, 4 for header's sector label
void mfm_decode_bits(const uint32_t* _Nonnull input, uint32_t* _Nonnull output, size_t data_size)
{
    const uint32_t* in_even = input + data_size;
    const uint32_t* in_odd = input;
    
    /* the decoding is made here long by long : with data_size/4 iterations */
    while (data_size-- > 0) {
        const uint32_t odd_bits = *in_odd++;
        const uint32_t even_bits = *in_even++;

        /*
         * MFM decoding, explained on one byte here (o and e will produce t) :
         * the MFM bytes 'abcdefgh' == o and 'ijklmnop' == e will become
         * e & 0x55U = '0j0l0n0p'
         * ( o & 0x55U) << 1 = 'b0d0f0h0'
         * '0j0l0n0p' | 'b0d0f0h0' = 'bjdlfnhp' == t
         */
        *output++ = (even_bits & MASK) | ((odd_bits & MASK) << 1u);
    }
}


// MFM encodes a sector
// Based on the sample code in Amiga-Magazin, 4/1989, p. 110ff
void mfm_encode_bits(const uint32_t* _Nonnull input, uint32_t* _Nonnull output, size_t data_size)
{
    uint32_t* out_even = output + data_size;
    uint32_t* out_odd = output;

    //    user's data bit      MFM coded bits
    //    ---------------      --------------
    //    1                    01
    //    0                    10 if following a 0 data bit
    //    0                    00 if following a 1 data bit
    while (data_size-- > 0) {
        const uint32_t in_bits = *input++;

        *out_even++ = (in_bits & MASK);
        *out_odd++ = ((in_bits >> 1u) & MASK);
    }
}

// Expects that there is one uint16_t in front of 'data'. 'data_size' is the
// number of 16bit words in the buffer.
// Adjusts the clock bits in the data buffer of 16bit words to make them conforming
// with the MFM encoding rules. Should be executed on a data buffer that was
// generated by the mfm_encode_bits() function.
// Based on sample code #5 in Amiga-Magazin, 4/1989, p. 110
void mfm_adj_clock_bits(uint16_t* _Nonnull data, size_t data_size)
{
    uint16_t prev_word = *(data - 1);

    while (data_size-- > 0) {
        const uint16_t in_word = *data;
        const uint16_t masked = in_word ^ MASK;
        const uint16_t max_clock_bits = (masked << 1u) & ((masked >> 1u) | 0x8000);
        uint16_t out_word = in_word | max_clock_bits;

        if ((prev_word & 0x01) == 1) {
            out_word &= ~0x8000;
        }

        prev_word = out_word;
        *data++ = out_word;
    }
}

// MFM decodes a sector.
// See "Amiga Disk Drives Inside and Out" by Abraham, Grote Gelfand, page 180, 181
// \param input    MFM coded data buffer (size == 2*data_size)
// \param data_size size in long, 1 for header's info, 4 for header's sector label
uint32_t mfm_checksum(const uint32_t* _Nonnull input, size_t data_size)
{
    uint32_t sum = 0;

    while (data_size-- > 0) {
        sum ^= *input++;
    }
    
    return sum & MASK;
}

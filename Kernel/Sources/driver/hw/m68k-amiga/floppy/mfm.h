//
//  mfm.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef mfm_h
#define mfm_h

#include <stdint.h>
#include <stddef.h>

struct ADF_MFMPhysicalSector;

extern void mfm_decode_bits(const uint32_t* _Nonnull input, uint32_t* _Nonnull output, size_t data_size);
extern void mfm_encode_bits(const uint32_t* _Nonnull input, uint32_t* _Nonnull output, size_t data_size);
// Expects that there is one uint16_t in front of 'data'
extern void mfm_adj_clock_bits(uint16_t* _Nonnull data, size_t data_size);

extern uint32_t mfm_checksum(const uint32_t* _Nonnull input, size_t data_size);

#endif /* mfm_h */

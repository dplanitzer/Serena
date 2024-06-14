//
//  mfm.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef mfm_h
#define mfm_h

#include <klib/klib.h>

extern void mfm_decode_sector(const uint32_t* input, uint32_t* output, int data_size);
extern void mfm_encode_sector(const uint32_t* input, uint32_t* output, int data_size);

#endif /* mfm_h */

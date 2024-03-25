//
//  Bits.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Bits.h"
#include <klib/Memory.h>


// Sets 'nbits' bits starting at 'pBits'.
void Bits_SetRange(const BitPointer pBits, size_t nbits)
{
    if (nbits == 0) {
        return;
    }
    
    const BitPointer pLastBit = BitPointer_AddBitOffset(pBits, nbits - 1);
    
    if (pBits.bytePointer == pLastBit.bytePointer) {
        for (int i = pBits.bitIndex; i <= pLastBit.bitIndex; i++) {
            *pBits.bytePointer |= (1 << (7 - i));
        }
    } else {
        char* middle_start_p = pBits.bytePointer;
        char* middle_end_p = pLastBit.bytePointer;
        
        // first byte
        if (pBits.bitIndex > 0) {
            *pBits.bytePointer |= (0xffu >> pBits.bitIndex);
            middle_start_p++;
        }
        
        // last byte
        if (pLastBit.bitIndex < 7) {
            *pLastBit.bytePointer |= (0xffu << (7 - pLastBit.bitIndex));
        } else {
            middle_end_p++;
        }
        
        // middle range
        memset(middle_start_p, 0xff, middle_end_p - middle_start_p);
    }
}

// Sets 'nbits' bits starting at 'pBits'.
void Bits_ClearRange(const BitPointer pBits, size_t nbits)
{
    if (nbits == 0) {
        return;
    }
    
    const BitPointer pLastBit = BitPointer_AddBitOffset(pBits, nbits - 1);
    
    if (pBits.bytePointer == pLastBit.bytePointer) {
        for (int i = pBits.bitIndex; i <= pLastBit.bitIndex; i++) {
            *pBits.bytePointer &= ~(1 << (7 - i));
        }
    } else {
        char* middle_start_p = pBits.bytePointer;
        char* middle_end_p = pLastBit.bytePointer;
        
        // first byte
        if (pBits.bitIndex > 0) {
            *pBits.bytePointer &= ~(0xffu >> pBits.bitIndex);
            middle_start_p++;
        }
        
        // last byte
        if (pLastBit.bitIndex < 7) {
            *pLastBit.bytePointer &= ~(0xffu << (7 - pLastBit.bitIndex));
        } else {
            middle_end_p++;
        }
        
        // middle range
        memset(middle_start_p, 0, middle_end_p - middle_start_p);
    }
}

// Copies the bit range with length 'nbits' from 'pSrcBits' to 'pDstBits'.
void Bits_CopyRange(BitPointer pDstBits, const BitPointer pSrcBits, size_t nbits)
{
    if (nbits == 0 || BitPointer_Equals(pDstBits, pSrcBits)) {
        return;
    }
    
    const BitPointer pSrcLastBit = BitPointer_AddBitOffset(pSrcBits, nbits - 1);
    const BitPointer pDstLastBit = BitPointer_AddBitOffset(pDstBits, nbits - 1);
    
    if (pSrcBits.bitIndex == pDstBits.bitIndex && nbits >= 8) {
        // The destination covers >= 1 byte and the start bit index for source and destination are the same.
        // This means that we can copy bytes 1:1 and we don't have to shift bits while copying.
        char src_first_byte = *pSrcBits.bytePointer;
        char src_last_byte = *pSrcLastBit.bytePointer;
        char dst_first_byte = *pDstBits.bytePointer;
        char dst_last_byte = *pDstLastBit.bytePointer;
        char* src_middle_start_p = pSrcBits.bytePointer;
        char* dst_middle_start_p = pDstBits.bytePointer;
        char* src_middle_end_p = pSrcLastBit.bytePointer;
        
        // first byte
        if (pSrcBits.bitIndex > 0) {
            char first_byte_mask = 0xffu >> pSrcBits.bitIndex;
            dst_first_byte &= ~first_byte_mask;
            dst_first_byte |= (src_first_byte & first_byte_mask);
            src_middle_start_p++; dst_middle_start_p++;
        }
        
        // last byte
        if (pSrcLastBit.bitIndex < 7) {
            char last_byte_mask = 0xffu << (7 - pSrcLastBit.bitIndex);
            dst_last_byte &= ~last_byte_mask;
            dst_last_byte |= (src_last_byte & last_byte_mask);
        } else {
            src_middle_end_p++;
        }
        
        // middle range
        memmove(dst_middle_start_p, src_middle_start_p, src_middle_end_p - src_middle_start_p);
        
        // write the partial first & last bytes
        if (pSrcBits.bitIndex > 0) {
            *pDstBits.bytePointer = dst_first_byte;
        }
        if (pSrcLastBit.bitIndex < 7) {
            *pDstLastBit.bytePointer = dst_last_byte;
        }
    }
    else if (BitPointer_GreaterEquals(pDstBits, pSrcBits) && BitPointer_LessEquals(pDstBits, pSrcLastBit)) {
        // The destination covers > 1 byte and we need to shift bits while copying because the start
        // source and destination bit indexes are different. Source and destination ranges also
        // overlap.
        BitPointer pSrcPtr = pSrcLastBit;
        BitPointer pDstPtr = pDstLastBit;
        
        for (int i = 0; i < nbits; i++) {
            Bits_Copy(pDstPtr, pSrcPtr);
            pSrcPtr = BitPointer_Decremented(pSrcPtr);
            pDstPtr = BitPointer_Decremented(pDstPtr);
        }
        
    }
    else {
        // The destination covers > 1 byte and we need to shift bits while copying because the start
        // source and destination bit indexes are different. Source and destination ranges do not
        // overlap.
        BitPointer pSrcPtr = pSrcBits;
        BitPointer pDstPtr = pDstBits;
        
        for (int i = 0; i < nbits; i++) {
            Bits_Copy(pDstPtr, pSrcPtr);
            pSrcPtr = BitPointer_Incremented(pSrcPtr);
            pDstPtr = BitPointer_Incremented(pDstPtr);
        }
    }
}

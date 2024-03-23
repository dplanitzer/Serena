//
//  Bytes.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Bytes.h"


// Copies 'n' contiguous bytes in memory from 'pSrc' to 'pDst'.
void* _Nonnull memmove(void* _Nonnull pDst, const void* _Nonnull pSrc, size_t n)
{
    if (pSrc == pDst || n == 0) {
        return pDst;
    }
    
    const char* src = (const char*)pSrc;
    char* dst = (char*)pDst;
    const char* src_upper = src + n;
    const char* src_upper_incl = src + n - 1;
    char* dst_upper_incl = dst + n - 1;
    
    if (src_upper_incl >= dst && src_upper_incl <= dst_upper_incl) {
        // - destination and source memory ranges intersect and the destination
        // range contains the source upper bound
        const char* src_p = src_upper_incl;
        char* dst_p = dst_upper_incl;
        
        while (src_p >= src) {
            *dst_p-- = *src_p--;
        }
    }
    else {
        // - source and destination memory ranges do not intersect
        // - destination and source memory ranges intersect and the destination
        // range contains the source lower bound
        unsigned int leadingByteCountSrc = ((unsigned long)src) & 0x03;
        unsigned int leadingByteCountDst = ((unsigned long)dst) & 0x03;
        
        if (leadingByteCountSrc == leadingByteCountDst) {
            if (leadingByteCountSrc > 0) { *dst++ = *src++; leadingByteCountSrc--; }
            if (leadingByteCountSrc > 0) { *dst++ = *src++; leadingByteCountSrc--; }
            if (leadingByteCountSrc > 0) { *dst++ = *src++; leadingByteCountSrc--; }
            
            const char* src_upper4 = (const char*) (((unsigned long)src_upper) & ~0x03);
            while (src < src_upper4) {
                *((uint32_t*)dst) = *((uint32_t*)src);
                dst += 4; src += 4;
            }
            
            if (src_upper > src_upper4) {
                if (src < src_upper) { *dst++ = *src++; }
                if (src < src_upper) { *dst++ = *src++; }
                if (src < src_upper) { *dst++ = *src++; }
            }
        }
        else {
            while (src < src_upper) {
                *dst++ = *src++;
            }
        }
    }

    return pDst;
}

void* _Nonnull memset(void* _Nonnull dst, int c, size_t count)
{
    uint8_t* p = (uint8_t*)dst;
    const uint8_t b = (uint8_t)c;

    // Don't bother optimizing too small requests. It would take more time to
    // handle misalignments, unrolling and trailing bytes than it takes to
    // bite the bullet and process individual bytes
    if (count < 16) {
        while (count-- > 0) {
            *p++ = b;
        }
        return dst;
    }


    // Align to the next 32bit boundary
    const size_t n_mod_bytes = ((uintptr_t)p & 3);
    if (n_mod_bytes > 0) {
        do {
            *p++ = b;
        } while ((uintptr_t)p & 3);

        count -= (4 - n_mod_bytes);
    }
    const uint8_t* const pe = p + count;


    // We know that we can do at least one 32bit op at this point
    uint32_t* p4 = (uint32_t*)p;
    const uint32_t b4 = (b << 24) | (b << 16) | (b << 8) | b;
    const uint32_t* const pe4 = p4 + (count >> 2);
    const uint32_t* const pe16 = (const uint32_t* const)((uintptr_t)pe4 &  ~15);
    if (((uintptr_t)pe16 - (uintptr_t)p4) >> 4) {
        while (p4 < pe16) {
            *p4++ = b;
            *p4++ = b;
            *p4++ = b;
            *p4++ = b;
        }
    }
    while (p4 < pe4) {
        *p4++ = b4;
    }


    // Write the remaining bytes
    p = (uint8_t*)p4;
    while (p < pe) {
        *p++ = b;
    }

    return dst;
}

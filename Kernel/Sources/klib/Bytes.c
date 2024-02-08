//
//  Bytes.c
//  Apollo
//
//  Created by Dietmar Planitzer on 3/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Bytes.h"
#include <klib/Assert.h>


// Scans the 'nbytes' contiguous bytes in memory starting at 'p' and returns
// the offset to the first byte that is equal to 'mark'. -1 if 'mark' does
// not appear in the given range.
int Bytes_FindFirst(const void* _Nonnull pBytes, int nbytes, Byte mark)
{
    const Byte* p = (const Byte*)pBytes;
    const Byte* cur_p = p;
    const Byte* end_p = p + nbytes;
    
    while (cur_p < end_p) {
        if (*cur_p == mark) {
            return cur_p - p;
        }
        cur_p++;
    }
    
    return -1;
}

// Scans the 'nbytes' contiguous bytes in memory starting at 'p' and returns
// the offset to the first byte that is not equal to 'mark'. -1 if 'mark'
// appears in the given range.
int Bytes_FindFirstNotEquals(const void* _Nonnull pBytes, int nbytes, Byte mark)
{
    const Byte* p = (const Byte*)pBytes;
    const Byte* cur_p = p;
    const Byte* end_p = p + nbytes;
    
    while (cur_p < end_p) {
        if (*cur_p != mark) {
            return cur_p - p;
        }
        cur_p++;
    }
    
    return -1;
}

// Scans the 'nbytes' contiguous bytes in memory starting at 'p' and returns
// the offset to the last byte that is equal to 'mark'. -1 if 'mark' does not
// appear in the given range.
int Bytes_FindLast(const void* _Nonnull pBytes, int nbytes, Byte mark)
{
    const Byte* p = (const Byte*)pBytes;
    const Byte* cur_p = p + nbytes - 1;
    
    while (cur_p >= p) {
        if (*cur_p == mark) {
            return cur_p - p;
        }
        cur_p--;
    }
    
    return -1;
}

// Scans the 'nbytes' contiguous bytes in memory starting at 'p' and returns
// the offset to the last byte that is not equal to 'mark'. -1 if 'mark'
// appears in the given range.
int Bytes_FindLastNotEquals(const void* _Nonnull pBytes, int nbytes, Byte mark)
{
    const Byte* p = (const Byte*)pBytes;
    const Byte* cur_p = p;
    
    while (cur_p >= p) {
        if (*cur_p != mark) {
            return cur_p - p;
        }
        cur_p--;
    }
    
    return -1;
}

// Compares the bytes at 'p1' with the bytes at 'p2' and returns the offset to
// the first byte that do not compare equal. -1 is returned if all bytes are
// equal.
int Bytes_FindFirstDifference(const void* _Nonnull p1, const void* _Nonnull p2, int len)
{
    const Byte* s1 = (const Byte*)p1;
    const Byte* s2 = (const Byte*)p2;
    const Byte* start_s1_p = s1;
    const Byte* end_s1_p = s1 + len;
    
    assert(len >= 0);
    while (s1 < end_s1_p) {
        if (*s1 != *s2) {
            return s1 - start_s1_p;
        }
        
        s1++; s2++;
    }
    
    return -1;
}

// Copies 'n' contiguous bytes in memory from 'pSrc' to 'pDst'.
void Bytes_CopyRange(void* _Nonnull pDst, const void* _Nonnull pSrc, int n)
{
    assert(n >= 0);
    if (pSrc == pDst || n == 0) {
        return;
    }
    
    const Byte* src = (const Byte*)pSrc;
    Byte* dst = (Byte*)pDst;
    const Byte* src_upper = src + n;
    const Byte* src_upper_incl = src + n - 1;
    Byte* dst_upper_incl = dst + n - 1;
    
    if (src_upper_incl >= dst && src_upper_incl <= dst_upper_incl) {
        // - destination and source memory ranges intersect and the destination
        // range contains the source upper bound
        const Byte* src_p = src_upper_incl;
        Byte* dst_p = dst_upper_incl;
        
        while (src_p >= src) {
            *dst_p-- = *src_p--;
        }
    }
    else {
        // - source and destination memory ranges do not intersect
        // - destination and source memory ranges intersect and the destination
        // range contains the source lower bound
        unsigned int leadingByteCountSrc = ((unsigned int)src) & 0x03;
        unsigned int leadingByteCountDst = ((unsigned int)dst) & 0x03;
        
        if (leadingByteCountSrc == leadingByteCountDst) {
            if (leadingByteCountSrc > 0) { *dst++ = *src++; leadingByteCountSrc--; }
            if (leadingByteCountSrc > 0) { *dst++ = *src++; leadingByteCountSrc--; }
            if (leadingByteCountSrc > 0) { *dst++ = *src++; leadingByteCountSrc--; }
            
            const Byte* src_upper4 = (const Byte*) (((unsigned int)src_upper) & ~0x03);
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
}

// Zeros out 'len' contiguous bytes in memory starting at 'pBytes'
void Bytes_ClearRange(void* _Nonnull pBytes, int len)
{
    assert(len >= 0);
    Byte* p = (Byte*)pBytes;
    const Byte* end = p + len;
    unsigned int leadingByteCount = ((unsigned int)p) & 0x03;
    
    if (leadingByteCount > 0) { *p++ = 0; leadingByteCount--; }
    if (leadingByteCount > 0) { *p++ = 0; leadingByteCount--; }
    if (leadingByteCount > 0) { *p++ = 0; leadingByteCount--; }
    
    const Byte* end4 = (const Byte*) (((unsigned int)end) & ~0x03);
    while (p < end4) {
        *((uint32_t*)p) = 0;
        p += 4;
    }
    
    if (end > end4) {
        if (p < end) { *p++ = 0; }
        if (p < end) { *p++ = 0; }
        if (p < end) { *p++ = 0; }
    }
}

// Sets all bytes in the given range to 'byte'
void Bytes_SetRange(void* _Nonnull pBytes, int len, Byte byte)
{
    Byte* p = (Byte*)pBytes;
    Byte* end = p + len;
    
    assert(len >= 0);
    while (p < end) {
        *p++ = byte;
    }
}

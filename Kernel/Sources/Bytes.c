//
//  Bytes.c
//  Apollo
//
//  Created by Dietmar Planitzer on 3/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Bytes.h"


// Scans the 'nbytes' consecutive bytes starting at 'p' and returns the offset
// to the first byte that is equal to 'mark'. -1 if 'mark' does not appear in
// the given range.
Int Bytes_FindFirst(const Byte* _Nonnull p, Int nbytes, Byte mark)
{
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

// Scans the 'nbytes' consecutive bytes starting at 'p' and returns the offset
// to the first byte that is not equal to 'mark'. -1 if 'mark' appears in the
// given range.
Int Bytes_FindFirstNotEquals(const Byte* _Nonnull p, Int nbytes, Byte mark)
{
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

// Scans the 'nbytes' consecutive bytes starting at 'p' and returns the offset
// to the last byte that is equal to 'mark'. -1 if 'mark' does not appear in
// the given range.
Int Bytes_FindLast(const Byte* _Nonnull p, Int nbytes, Byte mark)
{
    const Byte* cur_p = p + nbytes - 1;
    
    while (cur_p >= p) {
        if (*cur_p == mark) {
            return cur_p - p;
        }
        cur_p--;
    }
    
    return -1;
}

// Scans the 'nbytes' consecutive bytes starting at 'p' and returns the offset
// to the last byte that is not equal to 'mark'. -1 if 'mark' appears in the
// given range.
Int Bytes_FindLastNotEquals(const Byte* _Nonnull p, Int nbytes, Byte mark)
{
    const Byte* cur_p = p;
    
    while (cur_p >= p) {
        if (*cur_p != mark) {
            return cur_p - p;
        }
        cur_p--;
    }
    
    return -1;
}

// Compares the bytes at 's1' with the bytes at 's2' and returns the offset to
// the first byte that do not compare equal. -1 is returned if all bytes are
// equal.
Int Bytes_FindFirstDifference(const Byte* _Nonnull s1, const Byte* _Nonnull s2, Int len)
{
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

// Copies 'n' consecutive bytes from 'src' to 'dst'.
void Bytes_CopyRange(Byte* _Nonnull dst, const Byte* _Nonnull src, Int n)
{
    assert(n >= 0);
    if (src == dst || n == 0) {
        return;
    }
    
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
        UInt leadingByteCountSrc = ((UInt)src) & 0x03;
        UInt leadingByteCountDst = ((UInt)dst) & 0x03;
        
        if (leadingByteCountSrc == leadingByteCountDst) {
            if (leadingByteCountSrc > 0) { *dst++ = *src++; leadingByteCountSrc--; }
            if (leadingByteCountSrc > 0) { *dst++ = *src++; leadingByteCountSrc--; }
            if (leadingByteCountSrc > 0) { *dst++ = *src++; leadingByteCountSrc--; }
            
            const Byte* src_upper4 = (const Byte*) (((UInt)src_upper) & ~0x03);
            while (src < src_upper4) {
                *((UInt32*)dst) = *((UInt32*)src);
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

// Zeros out 'len' consectuive bytes starting at 'p'
void Bytes_ClearRange(Byte* _Nonnull p, Int len)
{
    assert(len >= 0);
    const Byte* end = p + len;
    UInt leadingByteCount = ((UInt)p) & 0x03;
    
    if (leadingByteCount > 0) { *p++ = 0; leadingByteCount--; }
    if (leadingByteCount > 0) { *p++ = 0; leadingByteCount--; }
    if (leadingByteCount > 0) { *p++ = 0; leadingByteCount--; }
    
    const Byte* end4 = (const Byte*) (((UInt)end) & ~0x03);
    while (p < end4) {
        *((UInt32*)p) = 0;
        p += 4;
    }
    
    if (end > end4) {
        if (p < end) { *p++ = 0; }
        if (p < end) { *p++ = 0; }
        if (p < end) { *p++ = 0; }
    }
}

// Sets all bytes in the given range to 'byte'
void Bytes_SetRange(Byte* _Nonnull p, Int len, Byte byte)
{
    Byte* end = p + len;
    
    assert(len >= 0);
    while (p < end) {
        *p++ = byte;
    }
}

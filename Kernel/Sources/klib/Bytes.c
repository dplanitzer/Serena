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
int Bytes_FindFirst(const void* _Nonnull pBytes, size_t nbytes, int mark)
{
    const char* p = (const char*)pBytes;
    const char* cur_p = p;
    const char* end_p = p + nbytes;
    
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
int Bytes_FindFirstNotEquals(const void* _Nonnull pBytes, size_t nbytes, int mark)
{
    const char* p = (const char*)pBytes;
    const char* cur_p = p;
    const char* end_p = p + nbytes;
    
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
int Bytes_FindLast(const void* _Nonnull pBytes, size_t nbytes, int mark)
{
    const char* p = (const char*)pBytes;
    const char* cur_p = p + nbytes - 1;
    
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
int Bytes_FindLastNotEquals(const void* _Nonnull pBytes, size_t nbytes, int mark)
{
    const char* p = (const char*)pBytes;
    const char* cur_p = p;
    
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
int Bytes_FindFirstDifference(const void* _Nonnull p1, const void* _Nonnull p2, size_t len)
{
    const char* s1 = (const char*)p1;
    const char* s2 = (const char*)p2;
    const char* start_s1_p = s1;
    const char* end_s1_p = s1 + len;
    
    while (s1 < end_s1_p) {
        if (*s1 != *s2) {
            return s1 - start_s1_p;
        }
        
        s1++; s2++;
    }
    
    return -1;
}

// Copies 'n' contiguous bytes in memory from 'pSrc' to 'pDst'.
void Bytes_CopyRange(void* _Nonnull pDst, const void* _Nonnull pSrc, size_t n)
{
    if (pSrc == pDst || n == 0) {
        return;
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
}

// Zeros out 'len' contiguous bytes in memory starting at 'pBytes'
void Bytes_ClearRange(void* _Nonnull pBytes, size_t len)
{
    char* p = (char*)pBytes;
    const char* end = p + len;
    unsigned int leadingByteCount = ((unsigned long)p) & 0x03;
    
    if (leadingByteCount > 0) { *p++ = 0; leadingByteCount--; }
    if (leadingByteCount > 0) { *p++ = 0; leadingByteCount--; }
    if (leadingByteCount > 0) { *p++ = 0; leadingByteCount--; }
    
    const char* end4 = (const char*) (((unsigned long)end) & ~0x03);
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
void Bytes_SetRange(void* _Nonnull pBytes, size_t len, int byte)
{
    char* p = (char*)pBytes;
    char* end = p + len;
    
    while (p < end) {
        *p++ = byte;
    }
}

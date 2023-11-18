//
//  Bytes.h
//  Apollo
//
//  Created by Dietmar Planitzer on 3/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Bytes_h
#define Bytes_h

#include <klib/Types.h>


// Scans the 'nbytes' contiguous bytes in memory starting at 'p' and returns
// the offset to the first byte that is equal to 'mark'. -1 if 'mark' does
// not appear in the given range.
extern Int Bytes_FindFirst(const void* _Nonnull p, Int nbytes, Byte mark);

// Scans the 'nbytes' contiguous bytes in memory starting at 'p' and returns
// the offset to the first byte that is not equal to 'mark'. -1 if 'mark'
// appears in the given range.
extern Int Bytes_FindFirstNotEquals(const void* _Nonnull p, Int nbytes, Byte mark);

// Scans the 'nbytes' contiguous bytes in memory starting at 'p' and returns
// the offset to the last byte that is equal to 'mark'. -1 if 'mark' does not
// appear in the given range.
extern Int Bytes_FindLast(const void* _Nonnull p, Int nbytes, Byte mark);

// Scans the 'nbytes' contiguous bytes in memory starting at 'p' and returns
// the offset to the last byte that is not equal to 'mark'. -1 if 'mark'
// appears in the given range.
extern Int Bytes_FindLastNotEquals(const void* _Nonnull p, Int nbytes, Byte mark);

// Compares the bytes at 'p1' with the bytes at 'p2' and returns the offset to
// the first byte that do not compare equal. -1 is returned if all bytes are
// equal.
extern Int Bytes_FindFirstDifference(const void* _Nonnull p1, const void* _Nonnull p2, Int len);

// Copies 'n' contiguous bytes in memory from 'pSrc' to 'pDst'.
extern void Bytes_CopyRange(void* _Nonnull pDst, const void* _Nonnull pSrc, Int n);

// Zeros out 'len' contiguous bytes in memory starting at 'pBytes'
extern void Bytes_ClearRange(void* _Nonnull pBytes, Int len);

// Sets all bytes in the given range to 'byte'
extern void Bytes_SetRange(void* _Nonnull pBytes, Int len, Byte c);

#endif /* Bytes_h */

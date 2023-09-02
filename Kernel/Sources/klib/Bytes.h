//
//  Bytes.h
//  Apollo
//
//  Created by Dietmar Planitzer on 3/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Bytes_h
#define Bytes_h

#include <Runtime.h>


// Scans the 'nbytes' contiguous bytes in memory starting at 'p' and returns
// the offset to the first byte that is equal to 'mark'. -1 if 'mark' does
// not appear in the given range.
extern Int Bytes_FindFirst(const Byte* _Nonnull p, Int nbytes, Byte mark);

// Scans the 'nbytes' contiguous bytes in memory starting at 'p' and returns
// the offset to the first byte that is not equal to 'mark'. -1 if 'mark'
// appears in the given range.
extern Int Bytes_FindFirstNotEquals(const Byte* _Nonnull p, Int nbytes, Byte mark);

// Scans the 'nbytes' contiguous bytes in memory starting at 'p' and returns
// the offset to the last byte that is equal to 'mark'. -1 if 'mark' does not
// appear in the given range.
extern Int Bytes_FindLast(const Byte* _Nonnull p, Int nbytes, Byte mark);

// Scans the 'nbytes' contiguous bytes in memory starting at 'p' and returns
// the offset to the last byte that is not equal to 'mark'. -1 if 'mark'
// appears in the given range.
extern Int Bytes_FindLastNotEquals(const Byte* _Nonnull p, Int nbytes, Byte mark);

// Compares the bytes at 's1' with the bytes at 's2' and returns the offset to
// the first byte that do not compare equal. -1 is returned if all bytes are
// equal.
extern Int Bytes_FindFirstDifference(const Byte* _Nonnull s1, const Byte* _Nonnull s2, Int len);

// Copies 'n' contiguous bytes in memory from 'src' to 'dst'.
extern void Bytes_CopyRange(Byte* _Nonnull dst, const Byte* _Nonnull src, Int n);

// Zeros out 'len' contiguous bytes in memory starting at 'p'
extern void Bytes_ClearRange(Byte* _Nonnull p, Int len);

// Sets all bytes in the given range to 'byte'
extern void Bytes_SetRange(Byte* _Nonnull p, Int len, Byte c);

#endif /* Bytes_h */

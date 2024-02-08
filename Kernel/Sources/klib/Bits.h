//
//  Bits.h
//  Apollo
//
//  Created by Dietmar Planitzer on 3/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Bits_h
#define Bits_h

#include <klib/Types.h>


typedef struct _BitPointer {
    Byte*   bytePointer;    // Pointer to the byte which holds the bit at bit location 'bitIndex'
    int     bitIndex;       // Index of the bit in the byte. Range [0, 7] with 0 == left-most bit and 7 == right-most bit
} BitPointer;


// Creates a bit pointer from the given byte pointer and bit offset. The bit
// offset is relative to the left-most bit in the byte that 'ptr' points to.
static inline BitPointer BitPointer_Make(Byte* ptr, int bitOffset)
{
    BitPointer p;
    
    p.bytePointer = ptr + (bitOffset >> 3);
    p.bitIndex = bitOffset & 0x07;
    return p;
}

// Adds the given bit offset to the bit pointer.
static inline BitPointer BitPointer_AddBitOffset(const BitPointer pBits, int bitOffset)
{
    BitPointer p;
    
    p.bytePointer = pBits.bytePointer + ((pBits.bitIndex + bitOffset) >> 3);
    p.bitIndex = (pBits.bitIndex + bitOffset) & 0x07;
    return p;
}

// Increments the given bit pointer by 1.
static inline BitPointer BitPointer_Incremented(const BitPointer pBits)
{
    BitPointer p;
    
    if (pBits.bitIndex < 7) {
        p.bytePointer = pBits.bytePointer;
        p.bitIndex = pBits.bitIndex + 1;
    } else {
        p.bytePointer = pBits.bytePointer + 1;
        p.bitIndex = 0;
    }
    return p;
}

// Decrements the given bit pointer by 1.
static inline BitPointer BitPointer_Decremented(const BitPointer pBits)
{
    BitPointer p;
    
    if (pBits.bitIndex > 0) {
        p.bytePointer = pBits.bytePointer;
        p.bitIndex = pBits.bitIndex - 1;
    } else {
        p.bytePointer = pBits.bytePointer - 1;
        p.bitIndex = 7;
    }
    return p;
}

// Returns true if both bit pointers are the same.
static inline bool BitPointer_Equals(BitPointer a, BitPointer b)
{
    return a.bytePointer == b.bytePointer && a.bitIndex == b.bitIndex;
}

static inline bool BitPointer_Less(BitPointer a, BitPointer b)
{
    return a.bytePointer < b.bytePointer || (a.bytePointer == b.bytePointer && a.bitIndex < b.bitIndex);
}

static inline bool BitPointer_LessEquals(BitPointer a, BitPointer b)
{
    return a.bytePointer < b.bytePointer || (a.bytePointer == b.bytePointer && a.bitIndex <= b.bitIndex);
}

static inline bool BitPointer_Greater(BitPointer a, BitPointer b)
{
    return a.bytePointer > b.bytePointer || (a.bytePointer == b.bytePointer && a.bitIndex > b.bitIndex);
}

static inline bool BitPointer_GreaterEquals(BitPointer a, BitPointer b)
{
    return a.bytePointer > b.bytePointer || (a.bytePointer == b.bytePointer && a.bitIndex >= b.bitIndex);
}



// Sets the bit at the given bit pointer location.
static inline void Bits_Set(const BitPointer pBits)
{
    *pBits.bytePointer |= (1 << (7 - pBits.bitIndex));
}

// Clears the bit at the given bit pointer location.
static inline void Bits_Clear(const BitPointer pBits)
{
    *pBits.bytePointer &= ~(1 << (7 - pBits.bitIndex));
}

// Returns true if the bit at the given bit pointer location is set; false otherwise.
static inline bool Bits_IsSet(const BitPointer pBits)
{
    return *pBits.bytePointer & (1 << (7 - pBits.bitIndex)) ? true : false;
}

// Scans the given bit array and returns the index to the first bit set. The
// bits in the array are numbered from 0 to nbits-1, with 0 being the first bit at 'pBits'.
// -1 is returned if no set bit is found.
extern int Bits_FindFirstSet(const BitPointer pBits, int nbits);

// Similar to Bits_FindFIrstSet() but scans from right to left.
extern int Bits_FindLastSet(const BitPointer pBits, int nbits);

// Scans the given bit array and returns the index to the first bit cleared. The
// bits in the array are numbered from 0 to nbits-1, with 0 being the first bit at 'pBits'.
// -1 is returned if no set bit is found.
extern int Bits_FindFirstCleared(const BitPointer pBits, int nbits);

// Similar to Bits_FindFirstCleared() but scans from right to left.
extern int Bits_FindLastCleared(const BitPointer pBits, int nbits);

// Sets 'nbits' bits starting at 'pBits'.
extern void Bits_SetRange(const BitPointer pBits, int nbits);

// Sets 'nbits' bits starting at 'pBits'.
extern void Bits_ClearRange(const BitPointer pBits, int nbits);

// Copies the bit at 'pSrcBit' to 'pDstBit'.
static inline void Bits_Copy(BitPointer pDstBit, const BitPointer pSrcBit)
{
    Byte dst_byte = *pDstBit.bytePointer;
    
    if (Bits_IsSet(pSrcBit)) {
        dst_byte |= (1 << (7 - pDstBit.bitIndex));
    } else {
        dst_byte &= ~(1 << (7 - pDstBit.bitIndex));
    }
    *pDstBit.bytePointer = dst_byte;
}

// Copies the bit range with length 'nbits' from 'pSrcBits' to 'pDstBits'.
extern void Bits_CopyRange(BitPointer pDstBits, const BitPointer pSrcBits, int nbits);

// Prints the given bit array
extern void Bits_Print(const BitPointer pBits, int nbits);

#endif /* Bits_h */

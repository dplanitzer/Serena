//
//  Header.h
//  Apollo
//
//  Created by Dietmar Planitzer on 5/30/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Header_h
#define Header_h

#include "Foundation.h"


// Ring buffer primitives. The ring buffer size must be a multiple of a power-of-2.
// See: <https://www.snellman.net/blog/archive/2016-12-13-ring-buffers/>
typedef struct _RingBuffer {
    Byte* _Nonnull  data;
    Int             capacity;
    Int             readIdx;
    Int             writeIdx;
} RingBuffer;


// Initializes the ring buffer to empty. 'capacity' is the buffer capacity in bytes.
// This value is rounded up to the next power of 2.
extern ErrorCode RingBuffer_Init(RingBuffer* _Nonnull pBuffer, Int capacity);

// Frees the ring buffer. This frees the ring buffer storage but not the elements
// stored inside the buffer.
extern void RingBuffer_Deinit(RingBuffer* _Nonnull pBuffer);

#define RingBuffer_MaskIndex(pBuffer, val)   ((val) & (pBuffer->capacity - 1))

// Returns true if the ring buffer is empty.
static inline Bool RingBuffer_IsEmpty(RingBuffer* _Nonnull pBuffer) {
    return pBuffer->readIdx == pBuffer->writeIdx;
}

// Returns the number of bytes stored in the ring buffer - aka the number of bytes
// that can be read from the ring buffer.
static inline Int RingBuffer_ReadableCount(RingBuffer* _Nonnull pBuffer) {
    return pBuffer->writeIdx - pBuffer->readIdx;
}

// Returns the number of bytes that can be written to the ring buffer.
static inline Int RingBuffer_WritableCount(RingBuffer* _Nonnull pBuffer) {
    return pBuffer->capacity - (pBuffer->writeIdx - pBuffer->readIdx);
}

// Removes all bytes from the ring buffer.
static inline void RingBuffer_RemoveAll(RingBuffer* _Nonnull pBuffer) {
    pBuffer->readIdx = 0;
    pBuffer->writeIdx = 0;
}

// Puts a single byte into the ring buffer.  Returns 0 if the buffer is empty and
// no byte has been copied out.
static inline Int RingBuffer_PutByte(RingBuffer* _Nonnull pBuffer, Byte byte) {
    if (RingBuffer_ReadableCount(pBuffer) < pBuffer->capacity) {
        pBuffer->data[RingBuffer_MaskIndex(pBuffer, pBuffer->writeIdx++)] = byte;
        return 1;
    } else {
        return 0;
    }
}

// Puts a sequence of bytes into the ring buffer by copying them. Returns the
// number of bytes that have been successfully copied into the buffer.
extern Int RingBuffer_PutBytes(RingBuffer* _Nonnull pBuffer, const Byte* _Nonnull pBytes, Int count);

// Gets a single byte from the ring buffer. Returns 0 if the buffer is empty and
// no byte has been copied out.
static inline Int RingBuffer_GetByte(RingBuffer* _Nonnull pBuffer, Byte* _Nonnull pByte) {
    if (!RingBuffer_IsEmpty(pBuffer)) {
        *pByte = pBuffer->data[RingBuffer_MaskIndex(pBuffer, pBuffer->readIdx++)];
        return 1;
    } else {
        return 0;
    }
}

// Gets a sequence of bytes from the ring buffer. The bytes are copied. Returns
// 0 if the buffer is empty. Returns the number of bytes that have been copied
// to 'pBytes'. 0 is returned if nothing has been copied because 'count' is 0
// or the ring buffer is empty.
extern Int RingBuffer_GetBytes(RingBuffer* _Nonnull pBuffer, Byte* _Nonnull pBytes, Int count);

#endif /* Header_h */

//
//  RingBuffer.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/30/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef RingBuffer_h
#define RingBuffer_h

#include <kern/errno.h>
#include <kern/types.h>


// Ring buffer primitives. The ring buffer size must be a multiple of a power-of-2.
// See: <https://www.snellman.net/blog/archive/2016-12-13-ring-buffers/>
typedef struct RingBuffer {
    char* _Nonnull  data;
    size_t          capacity;
    size_t          readIdx;
    size_t          writeIdx;
    unsigned int    flags;
} RingBuffer;


// Initializes the ring buffer to empty. 'capacity' is the buffer capacity in bytes.
// This value is rounded up to the next power of 2.
extern errno_t RingBuffer_Init(RingBuffer* _Nonnull self, size_t capacity);

// Initializes an empty ring buffer which stores all data in the provided buffer
// 'buf' with capacity 'capacity'. The ring buffer does not own 'buf' and the
// capacity must be a power-of-2 value.
extern void RingBuffer_InitWithBuffer(RingBuffer* _Nonnull self, char* _Nonnull buf, size_t capacity);

// Frees the ring buffer. This frees the ring buffer storage but not the elements
// stored inside the buffer.
extern void RingBuffer_Deinit(RingBuffer* _Nonnull self);

// Returns true if the ring buffer is empty.
static inline bool RingBuffer_IsEmpty(RingBuffer* _Nonnull self) {
    return self->readIdx == self->writeIdx;
}

// Returns the number of bytes stored in the ring buffer - aka the number of bytes
// that can be read from the ring buffer.
static inline size_t RingBuffer_ReadableCount(RingBuffer* _Nonnull self) {
    return self->writeIdx - self->readIdx;
}

// Returns the number of bytes that can be written to the ring buffer.
static inline size_t RingBuffer_WritableCount(RingBuffer* _Nonnull self) {
    return self->capacity - (self->writeIdx - self->readIdx);
}

// Removes all bytes from the ring buffer.
static inline void RingBuffer_RemoveAll(RingBuffer* _Nonnull self) {
    self->readIdx = 0;
    self->writeIdx = 0;
}

// Puts a single byte into the ring buffer.  Returns 0 if the buffer is empty and
// no byte has been copied out.
extern size_t RingBuffer_PutByte(RingBuffer* _Nonnull self, char byte);

// Puts a sequence of bytes into the ring buffer by copying them. Returns the
// number of bytes that have been successfully copied into the buffer.
extern size_t RingBuffer_PutBytes(RingBuffer* _Nonnull self, const void* _Nonnull pBytes, size_t count);

// Gets a single byte from the ring buffer. Returns 0 if the buffer is empty and
// no byte has been copied out.
extern size_t RingBuffer_GetByte(RingBuffer* _Nonnull self, char* _Nonnull pByte);

// Gets a sequence of bytes from the ring buffer. The bytes are copied. Returns
// 0 if the buffer is empty. Returns the number of bytes that have been copied
// to 'pBytes'. 0 is returned if nothing has been copied because 'count' is 0
// or the ring buffer is empty.
extern size_t RingBuffer_GetBytes(RingBuffer* _Nonnull self, void* _Nonnull pBytes, size_t count);

#endif /* RingBuffer_h */

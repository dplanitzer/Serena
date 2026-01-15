//
//  cbuf.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/30/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _CBUF_H
#define _CBUF_H

#include <stddef.h>
#include <ext/try.h>


// Ring buffer primitives. The ring buffer size must be a multiple of a power-of-2.
// See: <https://www.snellman.net/blog/archive/2016-12-13-ring-buffers/>
typedef struct cbuf {
    char* _Nonnull  data;
    size_t          capacity;
    size_t          readIdx;
    size_t          writeIdx;
    unsigned int    flags;
} cbuf_t;


// Initializes the ring buffer to empty. 'capacity' is the buffer capacity in bytes.
// This value is rounded up to the next power of 2.
extern errno_t cbuf_init(cbuf_t* _Nonnull self, size_t capacity);

// Initializes an empty ring buffer which stores all data in the provided buffer
// 'buf' with capacity 'capacity'. The ring buffer does not own 'buf' and the
// capacity must be a power-of-2 value.
extern void cbuf_init_extbuf(cbuf_t* _Nonnull _Restrict self, char* _Nonnull _Restrict buf, size_t capacity);

// Frees the ring buffer. This frees the ring buffer storage but not the elements
// stored inside the buffer.
extern void cbuf_deinit(cbuf_t* _Nonnull self);

// Returns true if the ring buffer is empty.
#define cbuf_empty(__self) \
(((__self)->readIdx == (__self)->writeIdx) ? true : false)

// Returns the number of bytes stored in the ring buffer - aka the number of bytes
// that can be read from the ring buffer.
#define cbuf_readable(__self) \
((__self)->writeIdx - (__self)->readIdx)


// Returns the number of bytes that can be written to the ring buffer.
#define cbuf_writable(__self) \
((__self)->capacity - ((__self)->writeIdx - (__self)->readIdx))


// Removes all bytes from the ring buffer.
#define cbuf_clear(__self) \
(__self)->readIdx = 0; \
(__self)->writeIdx = 0


#define __cbuf_mask_index(__self, __val) \
((__val) & ((__self)->capacity - 1))


// Puts a single byte into the ring buffer. Returns 1 if the byte has been
// copied to the buffer and 0 if the ring buffer is full.
#define _cbuf_put(__self, __byte) \
((cbuf_writable(__self) >= 1) ? (__self)->data[__cbuf_mask_index((__self), (__self)->writeIdx++)] = __byte, 1 : 0)

// Puts a single byte into the ring buffer. Returns 1 if the byte has been
// copied to the buffer and 0 if the ring buffer is full.
extern size_t cbuf_put(cbuf_t* _Nonnull self, char byte);

// Puts a sequence of bytes into the ring buffer by copying them. Returns the
// number of bytes that have been successfully copied into the buffer.
extern size_t cbuf_puts(cbuf_t* _Nonnull _Restrict self, const void* _Nonnull _Restrict pBytes, size_t count);


// Gets a single byte from the ring buffer. Returns 0 if the buffer is empty and
// no byte has been copied out.
#define _cbuf_get(__self, __pByte) \
((cbuf_readable(__self) >= 1) ? *(__pByte) = (__self)->data[__cbuf_mask_index(__self, (__self)->readIdx++)], 1 : 0)

// Gets a single byte from the ring buffer. Returns 0 if the buffer is empty and
// no byte has been copied out.
extern size_t cbuf_get(cbuf_t* _Nonnull _Restrict self, char* _Nonnull _Restrict pByte);

// Gets a sequence of bytes from the ring buffer. The bytes are copied. Returns
// 0 if the buffer is empty. Returns the number of bytes that have been copied
// to 'pBytes'. 0 is returned if nothing has been copied because 'count' is 0
// or the ring buffer is empty.
extern size_t cbuf_gets(cbuf_t* _Nonnull _Restrict self, void* _Nonnull _Restrict pBytes, size_t count);

#endif /* _CBUF_H */

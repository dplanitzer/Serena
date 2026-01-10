//
//  cbuf.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/30/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <ext/bit.h>
#include <kern/cbuf.h>
#include <kern/kalloc.h>
#include <kern/kernlib.h>

#define kFlag_OwnsBuffer    1

#define MASK_INDEX(__self, __val) \
((__val) & ((__self)->capacity - 1))


errno_t cbuf_init(cbuf_t* _Nonnull self, size_t capacity)
{
    self->capacity = pow2_ceil_sz(capacity);
    self->readIdx = 0;
    self->writeIdx = 0;
    self->flags = kFlag_OwnsBuffer;
    return kalloc(self->capacity, (void**) &self->data);
}

void cbuf_init_extbuf(cbuf_t* _Nonnull _Restrict self, char* _Nonnull _Restrict buf, size_t capacity)
{
    self->data = buf;
    self->capacity = capacity;
    self->readIdx = 0;
    self->writeIdx = 0;
}

void cbuf_deinit(cbuf_t* _Nonnull self)
{
    if ((self->flags & kFlag_OwnsBuffer) == kFlag_OwnsBuffer) {
        kfree(self->data);
    }
    self->data = NULL;
    self->capacity = 0;
    self->readIdx = 0;
    self->writeIdx = 0;
}

size_t cbuf_put(cbuf_t* _Nonnull self, char byte)
{
    if (cbuf_writable(self) >= 1) {
        self->data[MASK_INDEX(self, self->writeIdx++)] = byte;
        return 1;
    } else {
        return 0;
    }
}

size_t cbuf_puts(cbuf_t* _Nonnull _Restrict self, const void* _Nonnull _Restrict pBytes, size_t count)
{
    const int avail = cbuf_writable(self);
    
    if (count == 0 || avail == 0) {
        return 0;
    }
    
    const int nBytesToCopy = __min(avail, count);
    for (int i = 0; i < nBytesToCopy; i++) {
        self->data[MASK_INDEX(self, self->writeIdx + i)] = ((const char*)pBytes)[i];
    }
    self->writeIdx += nBytesToCopy;
    
    return nBytesToCopy;
}

size_t cbuf_get(cbuf_t* _Nonnull _Restrict self, char* _Nonnull _Restrict pByte)
{
    if (cbuf_readable(self) >= 1) {
        *pByte = self->data[MASK_INDEX(self, self->readIdx++)];
        return 1;
    } else {
        return 0;
    }
}

size_t cbuf_gets(cbuf_t* _Nonnull _Restrict self, void* _Nonnull _Restrict pBytes, size_t count)
{
    const int avail = cbuf_readable(self);
    
    if (count == 0 || avail == 0) {
        return 0;
    }
    
    const int nBytesToCopy = __min(avail, count);
    for (int i = 0; i < nBytesToCopy; i++) {
        ((char*)pBytes)[i] = self->data[MASK_INDEX(self, self->readIdx + i)];
    }
    self->readIdx += nBytesToCopy;
    
    return nBytesToCopy;
}

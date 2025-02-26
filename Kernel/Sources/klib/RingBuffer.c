//
//  RingBuffer.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/30/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "RingBuffer.h"

#define kFlag_OwnsBuffer    1

#define MASK_INDEX(__self, __val) \
((__val) & ((__self)->capacity - 1))


errno_t RingBuffer_Init(RingBuffer* _Nonnull self, size_t capacity)
{
    self->capacity = spow2_ceil(capacity);
    self->readIdx = 0;
    self->writeIdx = 0;
    self->flags = kFlag_OwnsBuffer;
    return kalloc(self->capacity, (void**) &self->data);
}

void RingBuffer_InitWithBuffer(RingBuffer* _Nonnull self, char* _Nonnull buf, size_t capacity)
{
    self->data = buf;
    self->capacity = capacity;
    self->readIdx = 0;
    self->writeIdx = 0;
}

void RingBuffer_Deinit(RingBuffer* _Nonnull self)
{
    if ((self->flags & kFlag_OwnsBuffer) == kFlag_OwnsBuffer) {
        kfree(self->data);
    }
    self->data = NULL;
    self->capacity = 0;
    self->readIdx = 0;
    self->writeIdx = 0;
}

size_t RingBuffer_PutByte(RingBuffer* _Nonnull self, char byte)
{
    if (RingBuffer_ReadableCount(self) < self->capacity) {
        self->data[MASK_INDEX(self, self->writeIdx++)] = byte;
        return 1;
    } else {
        return 0;
    }
}

size_t RingBuffer_PutBytes(RingBuffer* _Nonnull self, const void* _Nonnull pBytes, size_t count)
{
    const int avail = RingBuffer_WritableCount(self);
    
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

size_t RingBuffer_GetByte(RingBuffer* _Nonnull self, char* _Nonnull pByte)
{
    if (!RingBuffer_IsEmpty(self)) {
        *pByte = self->data[MASK_INDEX(self, self->readIdx++)];
        return 1;
    } else {
        return 0;
    }
}

size_t RingBuffer_GetBytes(RingBuffer* _Nonnull self, void* _Nonnull pBytes, size_t count)
{
    const int avail = RingBuffer_ReadableCount(self);
    
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

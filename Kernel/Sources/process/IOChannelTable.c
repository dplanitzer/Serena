//
//  IOChannelTable.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "IOChannelTable.h"
#include <kern/kalloc.h>
#include <kern/limits.h>


enum {
    kIOChannelTable_PageSize = 256
};


errno_t IOChannelTable_Init(IOChannelTable* _Nonnull self)
{
    decl_try_err();

    try(kalloc_cleared(sizeof(IOChannelRef) * kIOChannelTable_PageSize, (void**)&self->table));
    self->tableCapacity = kIOChannelTable_PageSize;
    mtx_init(&self->mtx);

catch:
    return err;
}

void IOChannelTable_Deinit(IOChannelTable* _Nonnull self)
{
    IOChannelTable_ReleaseAll(self);
    mtx_deinit(&self->mtx);
}

void IOChannelTable_ReleaseAll(IOChannelTable* _Nonnull self)
{
    IOChannelRef* table;
    size_t channelCount;

    mtx_lock(&self->mtx);
    table = self->table;
    channelCount = self->channelCount;
    self->table = NULL;
    self->tableCapacity = 0;
    self->channelCount = 0;
    mtx_unlock(&self->mtx);

    for (size_t i = 0, cc = 0; cc < channelCount; i++) {
        if (table[i]) {
            IOChannel_Release(table[i]);
            table[i] = NULL;
            cc++;
        }
    }
    kfree(table);
}

// Finds an empty slot in the I/O channel table and stores the I/O channel there.
// Returns the I/O channel descriptor and EOK on success and a suitable error and
// -1 otherwise. Note that this function takes ownership of the provided I/O
// channel.
errno_t IOChannelTable_AdoptChannel(IOChannelTable* _Nonnull self, IOChannelRef _Consuming _Nonnull pChannel, int * _Nonnull pOutIoc)
{
    decl_try_err();
    size_t ioc = 0;
    bool hasSlot = false;

    mtx_lock(&self->mtx);

    for (size_t i = 0; i < self->tableCapacity; i++) {
        if (self->table[i] == NULL) {
            ioc = i;
            hasSlot = true;
            break;
        }
    }

    if (!hasSlot || ioc > INT_MAX) {
        throw(EMFILE);
    }

    self->table[ioc] = pChannel;
    self->channelCount++;
    mtx_unlock(&self->mtx);

    *pOutIoc = ioc;
    return EOK;


catch:
    mtx_unlock(&self->mtx);
    *pOutIoc = -1;
    return err;
}

// Releases the I/O channel at the index 'ioc'. Releasing a channel means that
// the entry (name/descriptor) 'ioc' is removed from the table and that one
// strong reference is dropped. The channel is closed altogether if the last
// reference is removed. The error that this function returns is the error from
// the close operation. Note that this error is purely informative. The close
// will proceed and finish even if an error is encountered while doing so.
errno_t IOChannelTable_ReleaseChannel(IOChannelTable* _Nonnull self, int ioc)
{
    decl_try_err();
    IOChannelRef pChannel;

    // Do the actual channel release outside the table lock because the release
    // may take some time to execute. Ie it's synchronously draining some buffered
    // data.
    mtx_lock(&self->mtx);
    if (ioc < 0 || ioc > self->tableCapacity || self->table[ioc] == NULL) {
        throw(EBADF);
    }

    pChannel = self->table[ioc];
    self->table[ioc] = NULL;
    self->channelCount--;
    mtx_unlock(&self->mtx);

    return IOChannel_Release(pChannel);

catch:
    mtx_unlock(&self->mtx);
    return err;
}

// Returns the I/O channel that is named by 'ioc'. The channel is guaranteed to
// stay alive until it is relinquished. You should relinquish the channel by
// calling IOChannelTable_RelinquishChannel(). Returns the channel and EOK on
// success and a suitable error and NULL otherwise.
errno_t IOChannelTable_AcquireChannel(IOChannelTable* _Nonnull self, int ioc, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    IOChannelRef pChannel = NULL;

    mtx_lock(&self->mtx);
    if (ioc < 0 || ioc > self->tableCapacity || self->table[ioc] == NULL) {
        throw(EBADF);
    }

    pChannel = self->table[ioc];
    IOChannel_BeginOperation(pChannel);

catch:
    mtx_unlock(&self->mtx);
    *pOutChannel = pChannel;
    return err;
}

// Creates a new named reference of the I/O channel 'ioc'. The new descriptor/name
// value will be at least 'minIocValue'.
errno_t IOChannelTable_DupChannel(IOChannelTable* _Nonnull self, int ioc, int minIocValue, int * _Nonnull pOutNewIoc)
{
    decl_try_err();
    size_t newIoc = 0;
    bool hasSlot = false;

    mtx_lock(&self->mtx);

    if (ioc < 0 || ioc > self->tableCapacity || self->table[ioc] == NULL) {
        throw(EBADF);
    }

    for (size_t i = minIocValue; i < self->tableCapacity; i++) {
        if (self->table[i] == NULL) {
            newIoc = i;
            hasSlot = true;
            break;
        }
    }

    if (!hasSlot || newIoc > INT_MAX) {
        throw(EMFILE);
    }

    IOChannelRef pChannel = self->table[ioc];
    self->table[newIoc] = pChannel;
    IOChannel_Retain(pChannel);
    self->channelCount++;
    mtx_unlock(&self->mtx);

    *pOutNewIoc = newIoc;
    return EOK;


catch:
    mtx_unlock(&self->mtx);
    *pOutNewIoc = -1;
    return err;
}

// Assigns a new reference of the existing channel 'ioc' to 'targetIoc'. If
// 'targetIoc" names an existing I/O channel then this channel is implicitly
// closed.
errno_t IOChannelTable_DupChannelTo(IOChannelTable* _Nonnull self, int ioc, int targetIoc)
{
    decl_try_err();
    IOChannelRef pChannelToClose = NULL;

    mtx_lock(&self->mtx);

    if (ioc < 0 || ioc > self->tableCapacity || self->table[ioc] == NULL) {
        throw(EBADF);
    }
    if (targetIoc < 0 || targetIoc > self->tableCapacity) {
        throw(EBADF);
    }

    IOChannelRef pChannel = self->table[ioc];
    pChannelToClose = self->table[targetIoc];
    self->table[targetIoc] = pChannel;
    if (pChannelToClose == NULL) {
        self->channelCount++;
    }
    IOChannel_Retain(pChannel);

catch:
    mtx_unlock(&self->mtx);

    // We release the old channel outside the table lock because the release can
    // take a while. Ie buffered data is drained.
    // Also note that we always treat a close as successful because the channel
    // is in fact always closed even if it encounters a problem while doing so.
    if (pChannelToClose) {
        IOChannel_Release(pChannelToClose);
    }

    return err;
}

// Dups all I/O channels from 'pOther' to self. Expects that self is empty.
errno_t IOChannelTable_DupFrom(IOChannelTable* _Nonnull self, IOChannelTable* _Nonnull pOther)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    mtx_lock(&pOther->mtx);

    if (self->tableCapacity != pOther->tableCapacity) {
        throw(EINVAL);
    }

    for (size_t i = 0; i < pOther->channelCount; i++) {
        IOChannelRef pChannel = pOther->table[i];

        if (pChannel) {
            self->table[i] = IOChannel_Retain(pChannel);
            self->channelCount++;
        }
    }

catch:
    mtx_unlock(&pOther->mtx);
    mtx_unlock(&self->mtx);

    return err;
}

void IOChannelTable_ReleaseExecChannels(IOChannelTable* _Nonnull self)
{
    size_t channelCount;

    mtx_lock(&self->mtx);
    for (size_t i = 3, cc = 3; cc < self->channelCount; i++) {
        if (self->table[i]) {
            IOChannel_Release(self->table[i]);
            self->table[i] = NULL;
            cc++;
        }
    }
    mtx_unlock(&self->mtx);
}

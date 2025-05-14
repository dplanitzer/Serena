//
//  IOChannelTable.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "IOChannelTable.h"
#include <kern/kalloc.h>


enum {
    kIOChannelTable_PageSize = 256
};


errno_t IOChannelTable_Init(IOChannelTable* _Nonnull self)
{
    decl_try_err();

    try(kalloc_cleared(sizeof(IOChannelRef) * kIOChannelTable_PageSize, (void**)&self->table));
    self->tableCapacity = kIOChannelTable_PageSize;
    Lock_Init(&self->lock);

catch:
    return err;
}

void IOChannelTable_Deinit(IOChannelTable* _Nonnull self)
{
    IOChannelRef* table;
    size_t channelCount;

    Lock_Lock(&self->lock);
    table = self->table;
    channelCount = self->channelCount;
    self->table = NULL;
    self->tableCapacity = 0;
    self->channelCount = 0;
    Lock_Unlock(&self->lock);

    for (size_t i = 0, cc = 0; cc < channelCount; i++) {
        if (table[i]) {
            IOChannel_Release(table[i]);
            table[i] = NULL;
            cc++;
        }
    }
    kfree(table);

    Lock_Deinit(&self->lock);
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

    Lock_Lock(&self->lock);

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
    Lock_Unlock(&self->lock);

    *pOutIoc = ioc;
    return EOK;


catch:
    Lock_Unlock(&self->lock);
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
    Lock_Lock(&self->lock);
    if (ioc < 0 || ioc > self->tableCapacity || self->table[ioc] == NULL) {
        throw(EBADF);
    }

    pChannel = self->table[ioc];
    self->table[ioc] = NULL;
    self->channelCount--;
    Lock_Unlock(&self->lock);

    return IOChannel_Release(pChannel);

catch:
    Lock_Unlock(&self->lock);
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

    Lock_Lock(&self->lock);
    if (ioc < 0 || ioc > self->tableCapacity || self->table[ioc] == NULL) {
        throw(EBADF);
    }

    pChannel = self->table[ioc];
    IOChannel_BeginOperation(pChannel);

catch:
    Lock_Unlock(&self->lock);
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

    Lock_Lock(&self->lock);

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
    Lock_Unlock(&self->lock);

    *pOutNewIoc = newIoc;
    return EOK;


catch:
    Lock_Unlock(&self->lock);
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

    Lock_Lock(&self->lock);

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
    Lock_Unlock(&self->lock);

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

    Lock_Lock(&self->lock);
    Lock_Lock(&pOther->lock);

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
    Lock_Unlock(&pOther->lock);
    Lock_Unlock(&self->lock);

    return err;
}

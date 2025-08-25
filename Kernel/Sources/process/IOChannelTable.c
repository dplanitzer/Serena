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
#include <kern/string.h>


#define INITIAL_SIZE    32
#define GROW_SIZE       64
#define MAX_FD_COUNT    512


void IOChannelTable_Init(IOChannelTable* _Nonnull self)
{
    self->table = NULL;
    self->table_size = 0;
    self->max_fd_num = -1;
    mtx_init(&self->mtx);
}

void IOChannelTable_Deinit(IOChannelTable* _Nonnull self)
{
    IOChannelTable_ReleaseAll(self);
    mtx_deinit(&self->mtx);
}

void IOChannelTable_ReleaseAll(IOChannelTable* _Nonnull self)
{
    IOChannelRef* table;
    int max_fd_num;

    mtx_lock(&self->mtx);
    table = self->table;
    max_fd_num = self->max_fd_num;

    self->table = NULL;
    self->table_size = 0;
    self->max_fd_num = -1;
    mtx_unlock(&self->mtx);

    for (int i = 0; i < max_fd_num; i++) {
        if (table[i]) {
            IOChannel_Release(table[i]);
            table[i] = NULL;
        }
    }
    kfree(table);
}

static errno_t _ioct_ensure_size(IOChannelTable* _Nonnull _Locked self, int max_fd_needed)
{
    if (self->table_size > max_fd_needed) {
        return EOK;
    }


    int new_table_size;
    if (self->table) {
        new_table_size = self->table_size + __max(max_fd_needed + 1 - self->table_size, GROW_SIZE);
    }
    else {
        new_table_size = __max(max_fd_needed + 1, INITIAL_SIZE);
    }

    new_table_size = __min(new_table_size, MAX_FD_COUNT);
    if (max_fd_needed >= new_table_size) {
        return EMFILE;
    }


    void* new_table = NULL;
    const errno_t err = kalloc_cleared(sizeof(IOChannelRef) * new_table_size, &new_table);
    if (err != EOK) {
        return err;
    }


    if (self->table) {
        memcpy(new_table, self->table, sizeof(IOChannelRef) * self->table_size);
        kfree(self->table);
    }
    self->table = new_table;
    self->table_size = new_table_size;

    return EOK;
}

static errno_t _ioct_alloc_fd(IOChannelTable* _Nonnull _Locked self, int min_fd, int* _Nonnull out_fd)
{
    int new_fd = -1;

    for (;;) {
        for (int i = min_fd; i < self->table_size; i++) {
            if (self->table[i] == NULL) {
                new_fd = i;
                break;
            }
        }

        if (new_fd >= 0) {
            break;
        }


        const errno_t err = _ioct_ensure_size(self, self->table_size + 1);
        if (err != EOK) {
            *out_fd = -1;
            return err;
        }
    }

    self->max_fd_num = __max(self->max_fd_num, new_fd);
    *out_fd = new_fd;
    return EOK;
}

static IOChannelRef _Nullable _ioct_free_fd(IOChannelTable* _Nonnull _Locked self, int fd)
{
    IOChannelRef ch = self->table[fd];

    self->table[fd] = NULL;

    if (fd == self->max_fd_num) {
        for (int i = self->max_fd_num; i >= 0 && self->table[i] == NULL; i--) {
            self->max_fd_num--;
        }
    }

    return ch;
}

// Finds an empty slot in the I/O channel table and stores the I/O channel there.
// Returns the I/O channel descriptor and EOK on success and a suitable error and
// -1 otherwise. Note that this function takes ownership of the provided I/O
// channel.
errno_t IOChannelTable_AdoptChannel(IOChannelTable* _Nonnull self, IOChannelRef _Consuming _Nonnull pChannel, int * _Nonnull pOutIoc)
{
    mtx_lock(&self->mtx);

    int new_fd = -1;
    const errno_t err = _ioct_alloc_fd(self, 0, &new_fd);
    if (err == EOK) {
        self->table[new_fd] = pChannel;
    }

    mtx_unlock(&self->mtx);

    *pOutIoc = new_fd;
    return err;
}

// Releases the I/O channel at the index 'fd'. Releasing a channel means that
// the entry (name/descriptor) 'fd' is removed from the table and that one
// strong reference is dropped. The channel is closed altogether if the last
// reference is removed. The error that this function returns is the error from
// the close operation. Note that this error is purely informative. The close
// will proceed and finish even if an error is encountered while doing so.
errno_t IOChannelTable_ReleaseChannel(IOChannelTable* _Nonnull self, int fd)
{
    IOChannelRef ch = NULL;

    // Do the actual channel release outside the table lock because the release
    // may take some time to execute. Ie it's synchronously draining some buffered
    // data.
    mtx_lock(&self->mtx);

    if (fd >= 0 && fd <= self->max_fd_num && self->table[fd]) {
        ch = _ioct_free_fd(self, fd);
    }

    mtx_unlock(&self->mtx);

    return (ch) ? IOChannel_Release(ch) : EBADF;
}

// Returns the I/O channel that is named by 'fd'. The channel is guaranteed to
// stay alive until it is relinquished. You should relinquish the channel by
// calling IOChannel_EndOperation(). Returns the channel and EOK on
// success and a suitable error and NULL otherwise.
errno_t IOChannelTable_AcquireChannel(IOChannelTable* _Nonnull self, int fd, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    IOChannelRef ch = NULL;

    mtx_lock(&self->mtx);

    if (fd >= 0 && fd <= self->max_fd_num && self->table[fd]) {
        ch = self->table[fd];
        IOChannel_BeginOperation(ch);
    }
    
    mtx_unlock(&self->mtx);
    
    *pOutChannel = ch;
    return (ch) ? EOK : EBADF;
}

// Creates a new named reference of the I/O channel 'fd'. The new descriptor/name
// value will be at least 'min_fd'.
errno_t IOChannelTable_DupChannel(IOChannelTable* _Nonnull self, int fd, int min_fd, int * _Nonnull pOutNewIoc)
{
    decl_try_err();
    int new_fd = -1;

    mtx_lock(&self->mtx);

    if (fd >= 0 && fd <= self->max_fd_num && self->table[fd]) {
        err = _ioct_alloc_fd(self, min_fd, &new_fd);
        if (err == EOK) {
            self->table[new_fd] = IOChannel_Retain(self->table[fd]);
        }
    }

    mtx_unlock(&self->mtx);

    *pOutNewIoc = new_fd;
    return err;
}

// Assigns a new reference of the existing channel 'fd' to 'target_fd'. If
// 'target_fd" names an existing I/O channel then this channel is implicitly
// closed.
errno_t IOChannelTable_DupChannelTo(IOChannelTable* _Nonnull self, int fd, int target_fd)
{
    decl_try_err();
    IOChannelRef ch_to_close = NULL;

    mtx_lock(&self->mtx);

    if ((fd >= 0 && fd <= self->max_fd_num && self->table[fd])
        && (target_fd >= 0 && target_fd <= self->max_fd_num)) {
        
        ch_to_close = self->table[target_fd];
        self->table[target_fd] = IOChannel_Retain(self->table[fd]);

        self->max_fd_num = __max(self->max_fd_num, target_fd);
    }
    else {
        err = EBADF;
    }

    mtx_unlock(&self->mtx);

    // We release the old channel outside the table lock because the release can
    // take a while. Ie buffered data is drained.
    // Also note that we always treat a close as successful because the channel
    // is in fact always closed even if it encounters a problem while doing so.
    if (ch_to_close) {
        IOChannel_Release(ch_to_close);
    }

    return err;
}

// Dups all I/O channels from 'other' to self. Expects that self is empty.
errno_t IOChannelTable_DupFrom(IOChannelTable* _Nonnull self, IOChannelTable* _Nonnull other)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    mtx_lock(&other->mtx);

    assert(self->max_fd_num == -1);

    err = _ioct_ensure_size(self, other->max_fd_num);
    if (err == EOK) {
        for (int other_i = 0, my_i = 0; other_i <= other->max_fd_num; other_i++) {
            if (other->table[other_i]) {
                self->table[my_i++] = IOChannel_Retain(other->table[other_i]);
                self->max_fd_num = __max(self->max_fd_num, my_i);
            }
        }
    }

    mtx_unlock(&other->mtx);
    mtx_unlock(&self->mtx);

    return err;
}

void IOChannelTable_ReleaseExecChannels(IOChannelTable* _Nonnull self)
{
    mtx_lock(&self->mtx);
    for (int i = 3; i <= self->max_fd_num; i++) {
        if (self->table[i]) {
            IOChannel_Release(self->table[i]);
            self->table[i] = NULL;
        }
    }

    self->max_fd_num = -1;
    for (int i = 0; i < 3; i++) {
        if (self->table[i]) {
            self->max_fd_num = __max(self->max_fd_num, i);
        }
    }
    mtx_unlock(&self->mtx);
}

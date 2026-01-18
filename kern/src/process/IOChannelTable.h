//
//  IOChannelTable.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef IOChannelTable_h
#define IOChannelTable_h

#include <filesystem/IOChannel.h>
#include <sched/mtx.h>


typedef struct IOChannelTable {
    IOChannelRef _Nullable * _Nonnull   table;
    int                                 table_size;
    int                                 max_fd_num;
    mtx_t                               mtx;
} IOChannelTable;


extern void IOChannelTable_Init(IOChannelTable* _Nonnull self);
extern void IOChannelTable_Deinit(IOChannelTable* _Nonnull self);

// Finds an empty slot in the I/O channel table and stores the I/O channel there.
// Returns the I/O channel descriptor and EOK on success and a suitable error and
// -1 otherwise. Note that this function takes ownership of the provided I/O
// channel.
extern errno_t IOChannelTable_AdoptChannel(IOChannelTable* _Nonnull self, IOChannelRef _Consuming _Nonnull pChannel, int * _Nonnull pOutIoc);

// Releases the I/O channel at the index 'fd'. Releasing a channel means that
// the entry (name/descriptor) 'fd' is removed from the table and that one
// strong reference is dropped. The channel is closed altogether if the last
// reference is removed. The error that this function returns is the error from
// the close operation. Note that this error is purely informative. The close
// will proceed and finish even if an error is encountered while doing so.
extern errno_t IOChannelTable_ReleaseChannel(IOChannelTable* _Nonnull self, int fd);

// Returns the I/O channel that is named by 'fd'. The channel is guaranteed to
// stay alive until it is relinquished. You should relinquish the channel by
// calling IOChannel_EndOperation(). Returns the channel and EOK on success and
// a suitable error and NULL otherwise.
extern errno_t IOChannelTable_AcquireChannel(IOChannelTable* _Nonnull self, int fd, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Creates a new named reference of the I/O channel 'fd'. The new descriptor/name
// value will be at least 'min_fd'.
extern errno_t IOChannelTable_DupChannel(IOChannelTable* _Nonnull self, int fd, int min_fd, int * _Nonnull pOutNewIoc);

// Assigns a new reference of the existing channel 'fd' to 'target_fd'. If
// 'target_fd" names an existing I/O channel then this channel is implicitly
// closed.
extern errno_t IOChannelTable_DupChannelTo(IOChannelTable* _Nonnull self, int fd, int target_fd);

// Dups all I/O channels from 'other' to self. Expects that self is empty.
extern errno_t IOChannelTable_DupFrom(IOChannelTable* _Nonnull self, IOChannelTable* _Nonnull other);

// Release all I/O channels.
extern void IOChannelTable_ReleaseAll(IOChannelTable* _Nonnull self);

// Release and close all I/O channels that should be closed on a proc_exec()
// call.
extern void IOChannelTable_ReleaseExecChannels(IOChannelTable* _Nonnull self);

#endif /* IOChannelTable_h */

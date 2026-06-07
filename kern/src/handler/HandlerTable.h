//
//  HandlerTable.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef HandlerTable_h
#define HandlerTable_h

#include <handler/Handler.h>
#include <sched/mtx.h>


typedef struct HandlerTable {
    HandlerRef _Nullable * _Nonnull table;
    int                             table_size;
    int                             max_fd_num;
    mtx_t                           mtx;
} HandlerTable;


extern void HandlerTable_Init(HandlerTable* _Nonnull self);
extern void HandlerTable_Deinit(HandlerTable* _Nonnull self);

// Finds an empty slot in the handler table and stores the handler there.
// Returns the handler descriptor and EOK on success and a suitable error and
// -1 otherwise. Note that this function takes ownership of the provided handler.
extern errno_t HandlerTable_AdoptHandler(HandlerTable* _Nonnull self, HandlerRef _Consuming _Nonnull hnd, int * _Nonnull pOutIoc);

// Releases the handler at the index 'fd'. Releasing a channel means that
// the entry (name/descriptor) 'fd' is removed from the table and that one
// strong reference is dropped. The channel is closed altogether if the last
// reference is removed. The error that this function returns is the error from
// the close operation. Note that this error is purely informative. The close
// will proceed and finish even if an error is encountered while doing so.
extern errno_t HandlerTable_ReleaseHandler(HandlerTable* _Nonnull self, int fd);

// Returns the handler that is named by 'fd'. The channel is guaranteed to
// stay alive until it is relinquished. You should relinquish the channel by
// calling Handler_EndOperation(). Returns the channel and EOK on success and
// a suitable error and NULL otherwise.
extern errno_t HandlerTable_AcquireHandler(HandlerTable* _Nonnull self, int fd, HandlerRef _Nullable * _Nonnull pOutHandler);

// Creates a new named reference of the handler 'fd'. The new descriptor/name
// value will be at least 'min_fd'.
extern errno_t HandlerTable_DupHandler(HandlerTable* _Nonnull self, int fd, int min_fd, int * _Nonnull pOutNewIoc);

// Assigns a new reference of the existing channel 'fd' to 'target_fd'. If
// 'target_fd" names an existing handler then this channel is implicitly
// closed.
extern errno_t HandlerTable_DupHandlerTo(HandlerTable* _Nonnull self, int fd, HandlerTable* _Nonnull other, int target_fd);

// Release all handlers.
extern void HandlerTable_ReleaseAll(HandlerTable* _Nonnull self);

// Releases and closes all handlers that don't have the O_PRSVEXEC flag set.
extern void HandlerTable_ReleaseHandlersOnExec(HandlerTable* _Nonnull self);

#endif /* HandlerTable_h */

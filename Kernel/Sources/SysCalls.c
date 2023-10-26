//
//  Syscalls.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/19/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "DriverManager.h"
#include "Process.h"
#include "IOResource.h"
#include "VirtualProcessor.h"


typedef struct _SYS_open_args {
    Int                         scno;
    const Character* _Nonnull   path;
    UInt                        options;
} SYS_open_args;

Int _SYSCALL_open(const SYS_open_args* _Nonnull pArgs)
{
    decl_try_err();
    IOResourceRef pConsole = NULL;
    IOChannelRef pChannel = NULL;
    Int desc;

    if (String_Equals(pArgs->path, "/dev/console")) {
        try_null(pConsole, (IOResourceRef) DriverManager_GetDriverForName(gDriverManager, kConsoleName), ENODEV);
        try(IOResource_Open(pConsole, pArgs->path, pArgs->options, &pChannel));
        try(Process_RegisterIOChannel(Process_GetCurrent(), pChannel, &desc));
        Object_Release(pChannel);

        return desc;
    }

    return ENODEV;

catch:
    Object_Release(pChannel);
    return -err;
}


typedef struct _SYS_close_args {
    Int scno;
    Int fd;
} SYS_close_args;

Int _SYSCALL_close(const SYS_close_args* _Nonnull pArgs)
{
    decl_try_err();
    IOChannelRef pChannel;

    try(Process_UnregisterIOChannel(Process_GetCurrent(), pArgs->fd, &pChannel));
    // The error that close() returns is purely advisory and thus we'll proceed
    // with releasing the resource in any case.
    err = IOChannel_Close(pChannel);
    Object_Release(pChannel);
    return -err;

catch:
    return -err;
}


typedef struct _SYS_read_args {
    Int                 scno;
    Int                 fd;
    Character* _Nonnull buffer;
    UByteCount          count;
} SYS_read_args;

ByteCount _SYSCALL_read(const SYS_read_args* _Nonnull pArgs)
{
    decl_try_err();
    IOChannelRef pChannel;
    ByteCount nb = 0;

    try(Process_CopyIOChannelForDescriptor(Process_GetCurrent(), pArgs->fd, &pChannel));
    nb = IOChannel_Read(pChannel, pArgs->buffer, __ByteCountByClampingUByteCount(pArgs->count));
    Object_Release(pChannel);
    return nb;

catch:
    Object_Release(pChannel);
    return -err;
}


typedef struct _SYS_write_args {
    Int                     scno;
    Int                     fd;
    const Byte* _Nonnull    buffer;
    UByteCount              count;
} SYS_write_args;

ByteCount _SYSCALL_write(const SYS_write_args* _Nonnull pArgs)
{
    decl_try_err();
    IOChannelRef pChannel;
    ByteCount nb = 0;

    try(Process_CopyIOChannelForDescriptor(Process_GetCurrent(), pArgs->fd, &pChannel));
    nb = IOChannel_Write(pChannel, pArgs->buffer, __ByteCountByClampingUByteCount(pArgs->count));
    Object_Release(pChannel);
    return nb;

catch:
    Object_Release(pChannel);
    return -err;
}


typedef struct _SYS_sleep_args {
    Int             scno;
    TimeInterval    ti;
} SYS_sleep_args;

Int _SYSCALL_sleep(const SYS_sleep_args* _Nonnull pArgs)
{
    const ErrorCode err = VirtualProcessor_Sleep(pArgs->ti);
    return (err == EOK) ? EOK : -err;
}


typedef struct _SYS_dispatch_async_args {
    Int                             scno;
    const Closure1Arg_Func _Nonnull userClosure;
} SYS_dispatch_async_args;

Int _SYSCALL_dispatch_async(const SYS_dispatch_async_args* pArgs)
{
    decl_try_err();

    throw_ifnull(pArgs->userClosure, EPARAM);
    try(Process_DispatchAsyncUser(Process_GetCurrent(), pArgs->userClosure));
    return EOK;

catch:
    return -err;
}


// Allocates more address space to the calling process. The address space is
// expanded by 'count' bytes. A pointer to the first byte in the newly allocated
// address space portion is return in 'pOutMem'. 'pOutMem' is set to NULL and a
// suitable error is returned if the allocation failed. 'count' must be greater
// than 0 and a multipler of the CPU page size.
typedef struct _SYS_alloc_address_space_args {
    Int                         scno;
    UByteCount                  nbytes;
    Byte * _Nullable * _Nonnull pOutMem;
} SYS_alloc_address_space_args;

Int _SYSCALL_alloc_address_space(SYS_alloc_address_space_args* _Nonnull pArgs)
{
    decl_try_err();

    if (pArgs->nbytes > BYTE_COUNT_MAX) {
        throw(E2BIG);
    }
    throw_ifnull(pArgs->pOutMem, EPARAM);

    try(Process_AllocateAddressSpace(Process_GetCurrent(),
        __ByteCountByClampingUByteCount(pArgs->nbytes),
        pArgs->pOutMem));
    return EOK;

catch:
    return -err;
}


typedef struct _SYS_exit_args {
    Int scno;
    Int status;
} SYS_exit_args;

Int _SYSCALL_exit(const SYS_exit_args* _Nonnull pArgs)
{
    // Trigger the termination of the process. Note that the actual termination
    // is done asynchronously. That's why we sleep below since we don't want to
    // return to user space anymore.
    Process_Terminate(Process_GetCurrent(), pArgs->status);


    // This wait here will eventually be aborted when the dispatch queue that
    // owns this VP is terminated. This interrupt will be caused by the abort
    // of the call-as-user and thus this system call will not return to user
    // space anymore. Instead it will return to the dispatch queue main loop.
    VirtualProcessor_Sleep(kTimeInterval_Infinity);
    return EOK;
}


// Spawns a new process which is made the child of the process that is invoking
// this system call. The 'argv' pointer points to a table that holds pointers to
// nul-terminated strings. The last entry in the table has to be NULL. All these
// strings are the command line arguments that should be passed to the new
// process.
typedef struct _SYS_spawn_process_args {
    Int                             scno;
    const SpawnArguments* _Nullable spawnArgs;
} SYS_spawn_process_args;

Int _SYSCALL_spawn_process(const SYS_spawn_process_args* pArgs)
{
    decl_try_err();

    throw_ifnull(pArgs->spawnArgs, EPARAM);
    try(Process_SpawnChildProcess(Process_GetCurrent(), pArgs->spawnArgs, NULL));

    return EOK;

catch:
    return -err;
}


Int _SYSCALL_getpid(void)
{
    return Process_GetId(Process_GetCurrent());
}


Int _SYSCALL_getppid(void)
{
    return Process_GetParentId(Process_GetCurrent());
}


Int _SYSCALL_getpargs(void)
{
    return (Int) Process_GetArgumentsBaseAddress(Process_GetCurrent());
}

//
//  sys_file.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <handler/DriverHandler.h>
#include <handler/InodeHandler.h>
#include <handler/HandlerTable.h>
#include <kern/kernlib.h>
#include <process/ProcessPriv.h>


SYSCALL_1(fd_close, int fd)
{
    ProcessRef pp = vp->proc;

    return HandlerTable_CloseHandler(&pp->HandlerTable, pa->fd);
}

SYSCALL_4(fd_read, int fd, void* _Nonnull buffer, size_t nBytesToRead, ssize_t* _Nonnull nBytesRead)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    HandlerRef hnd;

    if ((err = HandlerTable_CopyHandler(&pp->HandlerTable, pa->fd, NULL, &hnd)) == EOK) {
        err = Handler_Read(hnd, pa->buffer, __SSizeByClampingSize(pa->nBytesToRead), pa->nBytesRead);
        Object_Release(hnd);
    }
    return err;
}

SYSCALL_4(fd_write, int fd, const void* _Nonnull buffer, size_t nBytesToWrite, ssize_t* _Nonnull nBytesWritten)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    HandlerRef hnd;

    if ((err = HandlerTable_CopyHandler(&pp->HandlerTable, pa->fd, NULL, &hnd)) == EOK) {
        err = Handler_Write(hnd, pa->buffer, __SSizeByClampingSize(pa->nBytesToWrite), pa->nBytesWritten);
        Object_Release(hnd);
    }
    return err;
}

SYSCALL_4(fd_seek, int fd, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    HandlerRef hnd;

    if ((err = HandlerTable_CopyHandler(&pp->HandlerTable, pa->fd, NULL, &hnd)) == EOK) {
        err = Handler_Seek(hnd, pa->offset, pa->pOutNewPos, pa->whence);
        Object_Release(hnd);
    }
    return err;
}

SYSCALL_3(fd_setflags, int fd, int op, int flags)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    HandlerRef hnd;

    if ((err = HandlerTable_CopyHandler(&pp->HandlerTable, pa->fd, NULL, &hnd)) == EOK) {
        err = Handler_SetFlags(hnd, pa->op, pa->flags);
        Object_Release(hnd);
    }
    return err;
}

SYSCALL_3(ioctl, int fd, int cmd, va_list _Nullable ap)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    HandlerRef hnd;

    if ((err = HandlerTable_CopyHandler(&pp->HandlerTable, pa->fd, class(DriverHandler), &hnd)) == EOK) {
        err = DriverHandler_vIoctl(hnd, pa->cmd, pa->ap);
        Object_Release(hnd);
    }
    return err;
}

SYSCALL_2(fd_truncate, int fd, off_t length)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    HandlerRef hnd;

    if ((err = HandlerTable_CopyHandler(&pp->HandlerTable, pa->fd, class(InodeHandler), &hnd)) == EOK) {
        err = InodeHandler_Truncate(hnd, pa->length);
        Object_Release(hnd);
    }
    return err;
}

SYSCALL_2(fd_attr, int fd, fs_attr_t* _Nonnull attr)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    HandlerRef hnd;

    if ((err = HandlerTable_CopyHandler(&pp->HandlerTable, pa->fd, class(InodeHandler), &hnd)) == EOK) {
        err = InodeHandler_GetAttributes(hnd, pa->attr);
        Object_Release(hnd);
    }
    return err;
}

SYSCALL_3(fd_info, int fd, int flavor, fd_info_ref _Nonnull info)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    HandlerRef hnd;

    if ((err = HandlerTable_CopyHandler(&pp->HandlerTable, pa->fd, NULL, &hnd)) == EOK) {
        err = Handler_GetInfo(hnd, pa->flavor, pa->info);
        Object_Release(hnd);
    }
    return err;
}

SYSCALL_3(fd_dup, int fd, int min_fd, int* _Nonnull new_fd)
{
    ProcessRef pp = vp->proc;

    return HandlerTable_DupHandler(&pp->HandlerTable, pa->fd, pa->min_fd, pa->new_fd);
}

SYSCALL_3(fd_dup_to, int fd, int target_fd, int* _Nonnull new_fd)
{
    ProcessRef pp = vp->proc;

    return HandlerTable_DupHandlerTo(&pp->HandlerTable, pa->fd, &pp->HandlerTable, pa->target_fd);
}

//
//  HandlerTable.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "HandlerTable.h"
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <ext/math.h>
#include <kern/kalloc.h>
#include <kpi/fd.h>
#include <kpi/syslimits.h>


#define FDTAB_PAGE_SHIFT    7
#define FDTAB_PAGE_SIZE     (1 << FDTAB_PAGE_SHIFT)


void HandlerTable_Init(HandlerTable* _Nonnull self)
{
    self->table = NULL;
    self->table_size = 0;
    self->max_fd_num = -1;
    mtx_init(&self->mtx);
}

void HandlerTable_Deinit(HandlerTable* _Nonnull self)
{
    HandlerTable_CloseAll(self);
    mtx_deinit(&self->mtx);
}

static errno_t _ensure_fd_slot_exists(HandlerTable* _Nonnull _Locked self, int fd_slot)
{
    decl_try_err();
    void* new_table;

    if (self->table_size > fd_slot) {
        return EOK;
    }
    if (fd_slot > _FDNO_MAX) {
        return EMFILE;
    }


    const int new_page_count = (fd_slot >> FDTAB_PAGE_SHIFT) + 1;
    const int new_table_size = new_page_count * FDTAB_PAGE_SIZE;
    const int new_memblk_size = new_table_size * sizeof(HandlerRef);
    if (new_memblk_size < 0) {
        // overflow
        return EMFILE;
    }


    err = kalloc_cleared(new_memblk_size, &new_table);
    if (err == EOK) {
        if (self->table) {
            memcpy(new_table, self->table, sizeof(HandlerRef) * (self->max_fd_num + 1));
            kfree(self->table);
        }

        self->table = new_table;
        self->table_size = new_table_size;
    }

    return err;
}

static errno_t _alloc_fd_slot(HandlerTable* _Nonnull _Locked self, int min_fd, int* _Nonnull out_fd)
{
    decl_try_err();
    int new_fd = -1;

    do {
        for (int i = min_fd; i < self->table_size; i++) {
            if (self->table[i] == NULL) {
                *out_fd = i;
                self->max_fd_num = __max(self->max_fd_num, i);
                return EOK;
            }
        }

        err = _ensure_fd_slot_exists(self, self->table_size);
    } while (err == EOK);

    return err;
}

static void _clear_fd_slot(HandlerTable* _Nonnull _Locked self, int fd)
{
    self->table[fd] = NULL;

    if (fd == self->max_fd_num) {
        while (self->max_fd_num >= 0 && self->table[self->max_fd_num] == NULL) {
            self->max_fd_num--;
        }
    }
}

errno_t HandlerTable_AdoptHandler(HandlerTable* _Nonnull self, HandlerRef _Consuming _Nonnull hnd, int * _Nonnull pOutIoc)
{
    decl_try_err();
    int new_fd = -1;

    mtx_lock(&self->mtx);

    err = _alloc_fd_slot(self, 0, &new_fd);
    if (err == EOK) {
        self->table[new_fd] = hnd;
    }

    mtx_unlock(&self->mtx);

    *pOutIoc = new_fd;
    return err;
}

errno_t HandlerTable_CopyHandler(HandlerTable* _Nonnull self, int fd, Class* _Nullable pClass, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    decl_try_err();
    HandlerRef hnd = NULL;

    mtx_lock(&self->mtx);

    if (fd >= 0 && fd <= self->max_fd_num && self->table[fd]) {
        if (pClass == NULL || _dynamiccast((AnyRef)self->table[fd], pClass) != NULL) {
            hnd = Object_Retain(self->table[fd]);
        }
        else {
            err = EINVAL;
        }
    }
    else {
        err = EBADF;
    }
    
    mtx_unlock(&self->mtx);
    
    *pOutHandler = hnd;
    return err;
}

errno_t HandlerTable_DupHandler(HandlerTable* _Nonnull self, int fd, int min_fd, int * _Nonnull pOutNewIoc)
{
    decl_try_err();
    int new_fd = -1;

    mtx_lock(&self->mtx);

    if (fd >= 0 && fd <= self->max_fd_num && self->table[fd]) {
        err = _alloc_fd_slot(self, min_fd, &new_fd);
        if (err == EOK) {
            self->table[new_fd] = Object_Retain(self->table[fd]);
        }
    }

    mtx_unlock(&self->mtx);

    *pOutNewIoc = new_fd;
    return err;
}

errno_t HandlerTable_DupHandlerTo(HandlerTable* _Nonnull self, int fd, HandlerTable* _Nonnull other, int target_fd)
{
    decl_try_err();
    HandlerRef hnd_to_close = NULL;

    mtx_lock(&self->mtx);
    if (self != other) {
        mtx_lock(&other->mtx);
    }

    if (fd < 0 || fd > self->max_fd_num || self->table[fd] == NULL) {
        throw(EBADF);
    }
    if (target_fd < 0) {
        throw(EBADF);
    }


    if (target_fd <= other->max_fd_num) {
        // target-fd slot exists: close the existing fd and replace with the new one
        hnd_to_close = other->table[target_fd];
    }
    else {
        // target_fd slot does not exist: allocate it
        try(_ensure_fd_slot_exists(other, target_fd));
    }

    other->table[target_fd] = Object_Retain(self->table[fd]);
    other->max_fd_num = __max(other->max_fd_num, target_fd);


catch:
    if (self != other) {
        mtx_unlock(&other->mtx);
    }
    mtx_unlock(&self->mtx);

    // We close the old handler outside the table lock because the close can
    // take a while. Ie buffered data is drained.
    // Also note that we always treat a close as successful because the handler
    // is in fact always closed even if it encounters a problem while doing so.
    if (hnd_to_close) {
        Object_Release(hnd_to_close);
    }

    return err;
}

errno_t HandlerTable_CloseHandler(HandlerTable* _Nonnull self, int fd)
{
    decl_try_err();
    HandlerRef hnd = NULL;

    // Do the actual handler close outside the table lock because the close
    // may take some time to execute. Ie it's synchronously draining some buffered
    // data.
    mtx_lock(&self->mtx);

    if (fd >= 0 && fd <= self->max_fd_num && self->table[fd]) {
        hnd = self->table[fd];
        _clear_fd_slot(self, fd);
    }

    mtx_unlock(&self->mtx);

    if (hnd) {
        Object_Release(hnd);
    }
    else {
        err = EBADF;
    }

    return err;
}

void HandlerTable_CloseAll(HandlerTable* _Nonnull self)
{
    HandlerRef* table;
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
            Object_Release(table[i]);
            table[i] = NULL;
        }
    }
    kfree(table);
}

void HandlerTable_CloseHandlersOnExec(HandlerTable* _Nonnull self)
{
    int fd;

    mtx_lock(&self->mtx);
    fd = self->max_fd_num;

    while (fd >= 0) {
        HandlerRef hnd = self->table[fd];

        if (hnd && (Handler_GetFlags(hnd) & O_PRSVEXEC) == 0) {
            Object_Release(hnd);
            self->table[fd] = NULL;
        }
        fd--;
    }

    while (self->max_fd_num >= 0 && self->table[self->max_fd_num] == NULL) {
        self->max_fd_num--;
    }

    mtx_unlock(&self->mtx);
}

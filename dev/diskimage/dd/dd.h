//
//  dd.h
//  diskimage
//
//  Created by Dietmar Planitzer on 11/24/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef DD_H
#define DD_H

#include <stdio.h>
#include <dd/dd_path.h>
#include <dd/dd_rdir.h>
#include <dd/FSManager.h>
#include <kpi/attr.h>
#include <kpi/types.h>

// What a path refers to
enum {
    DD_KIND_ERROR = -1,     /* it couldn't be determined; errno is set */
    DD_KIND_NONE = 0,       /* nothing */
    DD_KIND_DIRECTORY = 1,
    DD_KIND_OTHER = 2       /* a file or anything else that isn't a directory */
};


// Disk image file descriptor (this one is a pointer)
typedef void* dd_fd_t;


// dd_open() flags
#define DD_RDONLY   0
#define DD_WRONLY   1
#define DD_RW       (DD_RDONLY | DD_WRONLY)


extern int dd_path_kind(FSManagerRef fsm, const char *path);

extern int dd_makedir(FSManagerRef fsm, const char *path, fs_perms_t perms);
extern int dd_remove(FSManagerRef fsm, const char *path);

extern int dd_chown(FSManagerRef fsm, const char *path, uid_t uid, gid_t gid);
extern int dd_attr(FSManagerRef fsm, const char *path, fs_attr_t *attr);

extern dd_fd_t dd_open(FSManagerRef fsm, const char *path, int flags);
extern int dd_close(dd_fd_t fd);

extern ssize_t dd_read(dd_fd_t fd, char *buf, size_t count);
extern ssize_t dd_write(dd_fd_t fd, const char *buf, size_t count);

#endif /* DD_H */

//
//  strerror.c
//  libc
//
//  Created by Dietmar Planitzer on 9/1/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <errno.h>


char * _Nonnull strerror(int err_no)
{
    static const char* gErrorDescs[] = {
        /*ENOMEM*/          "Out of memory",
        /*ENOMEDIUM*/       "No medium found",
        /*EDISKCHANGE*/     "Disk changed",
        /*ETIMEDOUT*/       "Timed out",
        /*ENODEV*/          "No such device",
        /*ERANGE*/          "Out of range parameter",
        /*EINTR*/           "Interrupted",
        /*EAGAIN*/          "Try again",
        /*EWOULDBLOCK*/     "Would block",
        /*EPIPE*/           "Broken pipe",
        /*EBUSY*/           "Resource is busy",
        /*ENOSYS*/          "Not a system call",
        /*EINVAL*/          "Invalid argument",
        /*EIO*/             "I/O error",
        /*EPERM*/           "Insufficient permissions",
        /*EDEADLK*/         "Deadlock detected",
        /*EDOM*/            "Argument outside of allowed domain",
        /*ENOEXEC*/         "Not an executable file",
        /*E2BIG*/           "Argument too big",
        /*ENOENT*/          "No such file or directory",
        /*ENOTBLK*/         "Not a block device",
        /*EBADF*/           "Not a valid descriptor",
        /*ECHILD*/          "Not a child process",
        /*ESRCH*/           "No such process",
        /*ESPIPE*/          "Not a file",
        /*ENOTDIR*/         "Not a directory",
        /*ENAMETOOLONG*/    "Name too long",
        /*EACCESS*/         "Insufficient permissions",
        /*EROFS*/           "Read-only file system",
        /*ENOSPC*/          "No space",
        /*EEXIST*/          "File exists",
        /*EOVERFLOW*/       "Value overflow",
        /*EFBIG*/           "File too big",
        /*EISDIR*/          "Is a directory",
        /*ENOTIOCTLCMD*/    "Not an IOCTL command",
        /*EILSEQ*/          "Invalid multibyte sequence",
        /*EMLINK*/          "Too many links",
        /*EMFILE*/          "Too many open I/O channels",
        /*EXDEV*/           "Cross-device link not permitted",
        /*ETERMINATED*/     "Terminated",
        /*ENOTSUP*/         "Operation not supported",
        /*ENXIO*/           "No such device or address",
        /*ENOTATTACHED*/    "Driver not attached",
    };

    if (err_no >= __EFIRST && err_no <= __ELAST) {
        return (char*) gErrorDescs[err_no - __EFIRST];
    } else {
        return "";
    }
}
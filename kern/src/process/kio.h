//
//  kio.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KIO_H
#define _KIO_H

#include <stdarg.h>
#include <stddef.h>
#include <ext/try.h>
#include <kobj/AnyRefs.h>
#include <kpi/attr.h>
#include <kpi/fd.h>
#include <kpi/types.h>


extern errno_t _kopen(ProcessRef _Nonnull pp, const char* _Nonnull path, int oflags, int* _Nonnull pOutIoc);
extern errno_t _kclose(ProcessRef _Nonnull pp, int fd);
extern errno_t _kread(ProcessRef _Nonnull pp, int fd, void* _Nonnull buffer, size_t nBytesToRead, ssize_t* _Nonnull nBytesRead);
extern errno_t _kwrite(ProcessRef _Nonnull pp, int fd, const void* _Nonnull buffer, size_t nBytesToWrite, ssize_t* _Nonnull nBytesWritten);
extern errno_t _kseek(ProcessRef _Nonnull pp, int fd, off_t offset, off_t* _Nullable pOutNewPos, int whence);
extern errno_t _kfcntl(ProcessRef _Nonnull pp, int fd, int cmd, int* _Nonnull pResult, va_list _Nullable ap);
extern errno_t _kioctl(ProcessRef _Nonnull pp, int fd, int cmd, va_list _Nullable ap);

extern errno_t _kftruncate(ProcessRef _Nonnull pp, int fd, off_t length);

extern errno_t _kfattr(ProcessRef _Nonnull pp, int fd, fs_attr_t* _Nonnull attr);
extern errno_t _kfinfo(ProcessRef _Nonnull pp, int fd, int flavor, fd_info_ref _Nonnull info);


#define kopen(path, oflags, pOutIoc) \
_kopen(gKernelProcess, path, oflags, pOutIoc)

#define kclose(fd) \
_kclose(gKernelProcess, fd)

#define kread(fd, buffer, nBytesToRead, nBytesRead) \
_kread(gKernelProcess, fd, buffer, nBytesToRead, nBytesRead)

#define kwrite(fd, buffer, nBytesToWrite, nBytesWritten) \
_kwrite(gKernelProcess, fd, buffer, nBytesToWrite, nBytesWritten)

#define kseek(fd, offset, pOutNewPos, whence) \
_kseek(gKernelProcess, fd, offset, pOutNewPos, whence)

#define kfcntl(fd, cmd, pResult, ap) \
_kfcntl(gKernelProcess, fd cmd, pResult, ap)

#define kioctl(fd, cmd, ap) \
_kioctl(gKernelProcess, fd, cmd, ap)

#define kftruncate(fd, length) \
_kftruncate(gKernelProcess, fd, length)

#define kfattr(fd, attr) \
_kfattr(gKernelProcess, fd, attr)

#define kfinfo(fd, flavor, info) \
_kfinfo(gKernelProcess, fd, flavor, info)


#endif /* _KIO_H */

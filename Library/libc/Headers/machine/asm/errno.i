;
;  errno.i
;  libsystem
;
;  Created by Dietmar Planitzer on 9/6/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

        ifnd __ERRNO_I
__ERRNO_I  set 1

EOK             equ 0
ENOMEM          equ 1
ENOMEDIUM       equ 2
EDISKCHANGE     equ 3
ETIMEDOUT       equ 4
ENODEV          equ 5
ERANGE          equ 6
EINTR           equ 7
EAGAIN          equ 8
EWOULDBLOCK     equ 9
EPIPE           equ 10
EBUSY           equ 11
ENOSYS          equ 12
EINVAL          equ 13
EIO             equ 14
EPERM           equ 15
EDEADLK         equ 16
EDOM            equ 17
ENOEXEC         equ 18
E2BIG           equ 19
ENOENT          equ 20
ENOTBLK         equ 21
EBADF           equ 22
ECHILD          equ 23
ESRCH           equ 24
ESPIPE          equ 25
ENOTDIR         equ 26
ENAMETOOLONG    equ 27
EACCESS         equ 28
EROFS           equ 29
ENOSPC          equ 30
EEXIST          equ 31
EOVERFLOW       equ 32
EFBIG           equ 33
EISDIR          equ 34
ENOTIOCTLCMD    equ 35
EILSEQ          equ 36
EMLINK          equ 37
EMFILE          equ 38
EXDEV           equ 39
ETERMINATED     equ 40
ENOTSUP         equ 41
ENXIO           equ 42

__EFIRST    equ 1
__ELAST     equ 42

        endif   ; __ERRNO_I

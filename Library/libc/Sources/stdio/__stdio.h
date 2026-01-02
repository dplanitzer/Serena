//
//  __stdio.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __STDIO_H
#define __STDIO_H 1

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <__stdlib.h>
#include <sys/mtx.h>


__CPP_BEGIN

typedef uint32_t __FILE_Mode;

enum {
    __kStreamMode_Read      = 0x01,     // Allow reading
    __kStreamMode_Write     = 0x02,     // Allow writing
    __kStreamMode_Append    = 0x04,     // Append to the file
    __kStreamMode_Exclusive = 0x08,     // Fail if file already exists instead of creating it
    __kStreamMode_Truncate  = 0x10,     // Truncate the file to 0 on open
    __kStreamMode_Create    = 0x20,     // Create the file if it doesn't exist
    __kStreamMode_Binary    =  0x0,     // Indicates that the data in the stream should be treated as binary (always active on Serena OS)
    __kStreamMode_Text      = 0x40,     // Indicates that the data in the stream should be treated as text
};

enum {
    __kStreamDirection_Unknown = 0,
    __kStreamDirection_In,
    __kStreamDirection_Out
};

enum {
    __kStreamOrientation_Unknown = 0,
    __kStreamOrientation_Byte,
    __kStreamOrientation_Wide
};


// FILE and subclassing:
// FILE is the abstract base class of all stream classes. A caveat that subclassers
// have to deal with is that freopen() exists. Freopen() must be able to convert
// any instance of a FILE subclass in place into a IOChannelFile subclass.
// Consequently every subclass of FILE must allocate enough space to hold an
// IOChannelFile even if the subclass itself is smaller.
// This macro here should be used to malloc() the correct amount of space for a
// FILE subclass. It ensures that there is always enough space to convert the
// allocated instance into an IOChannelFile.
// If you use a __fopen_xxx_init() function then you should always pass a pointer
// to a structure which is typed as the subclass that you want to initialize and
// you should never call freopen() on an instance that was created with an init()
// call. Note that there's no problem as long as you never call freopen() on a
// FILE subclass instance. 
#define SIZE_OF_FILE_SUBCLASS(__type) \
((sizeof(__type) > sizeof(__IOChannel_FILE)) ? sizeof(__type) : sizeof(__IOChannel_FILE))


typedef struct __IOChannel_FILE_Vars {
    int fd;
} __IOChannel_FILE_Vars;

typedef struct __IOChannel_FILE {
    FILE                    super;
    __IOChannel_FILE_Vars   v;
} __IOChannel_FILE;


typedef struct __Memory_FILE_Vars {
    char* _Nullable store;
    size_t          currentCapacity;        // current capacity of the backing store
    size_t          maximumCapacity;        // maximum permissible backing store capacity
    size_t          eofPosition;            // has to be >= storeCapacity
    size_t          currentPosition;        // is kept in the range 0...eofPosition
    struct {
        unsigned int freeOnClose:1;
    }               flags;
} __Memory_FILE_Vars;

typedef struct __Memory_FILE {
    FILE                super;
    __Memory_FILE_Vars  v;
} __Memory_FILE;


typedef int (*__file_func_t)(FILE* _Nonnull s);


extern const FILE_Callbacks __FILE_fd_callbacks;
extern FILE*    __gOpenFiles;
extern mtx_t    __gOpenFilesLock;

#define __init_open_files_lock() \
mtx_init(&__gOpenFilesLock)

#define __open_files_lock() \
mtx_lock(&__gOpenFilesLock)

#define __open_files_unlock() \
mtx_unlock(&__gOpenFilesLock)


extern int __fopen_parse_mode(const char* _Nonnull _Restrict mode, __FILE_Mode* _Nonnull _Restrict pOutMode);

extern int __fopen_init(FILE* _Nonnull _Restrict self, bool bFreeOnClose, void* _Nullable context, const FILE_Callbacks* _Nonnull _Restrict callbacks, __FILE_Mode sm);
extern int __fdopen_init(__IOChannel_FILE* _Nonnull self, bool bFreeOnClose, int ioc, __FILE_Mode sm);
extern int __fopen_filename_init(__IOChannel_FILE* _Nonnull _Restrict self, bool bFreeOnClose, const char * _Nonnull _Restrict filename, __FILE_Mode sm);
extern int __fopen_memory_init(__Memory_FILE* _Nonnull _Restrict self, bool bFreeOnClose, FILE_Memory * _Nonnull _Restrict mem, __FILE_Mode sm);
extern int __fopen_null_init(FILE* _Nonnull self, bool bFreeOnClose, __FILE_Mode sm);

extern FILE * _Nullable __fopen_null(const char* mode);


extern int __iterate_open_files(__file_func_t _Nonnull f);


extern ssize_t __fd_read(__IOChannel_FILE_Vars* _Nonnull self, void* buf, ssize_t nbytes);
extern ssize_t __mem_read(__Memory_FILE_Vars* _Nonnull mp, void* pBuffer, ssize_t nBytesToRead);


#define __fensure_byte_oriented(__self, __ret) \
if ((__self)->flags.orientation == __kStreamOrientation_Wide) { \
    (__self)->flags.hasError = 1; \
    return __ret; \
} \
(__self)->flags.orientation = __kStreamOrientation_Byte;

#define __fensure_direction(__self, dir, __ret) \
if ((__self)->flags.direction != (dir)) { \
    if (__fsetdir(__self, dir) == EOF) { \
        return __ret; \
    } \
}

#define __fensure_writeable(__self, __ret) \
if (((__self)->flags.mode & __kStreamMode_Write) == 0) { \
    (__self)->flags.hasError = 1; \
    return __ret; \
}

#define __fensure_readable(__self, __ret) \
if (((__self)->flags.mode & __kStreamMode_Read) == 0) { \
    (__self)->flags.hasError = 1; \
    return __ret; \
}

#define __fensure_no_eof_err(__self, __ret) \
if ((__self)->flags.hasEof || (__self)->flags.hasError) { \
    return __ret; \
}

#define __fensure_no_err(__self, __ret) \
if ((__self)->flags.hasError) { \
    return __ret; \
}

#define __fensure_seekable(__self, __ret) \
if ((__self)->cb.seek == NULL) { \
    errno = ESPIPE; \
    return __ret; \
}


#define __fdiscard_ugb(__self) \
(__self)->ugbCount = 0

extern int __fget_ugb(char* _Nonnull _Restrict pch, FILE * _Nonnull _Restrict s);

extern ssize_t __fwrite(FILE * _Nonnull _Restrict s, const void * _Restrict buffer, ssize_t nbytes);
extern ssize_t __fgetc(char* _Nonnull _Restrict pch, FILE * _Nonnull _Restrict sm);
extern ssize_t __fputc(char ch, FILE * _Nonnull s);
extern off_t __fgetlogicalpos(FILE * _Nonnull s);
extern int __fsetdir(FILE * _Nonnull s, int dir);
extern void __fdiscard(FILE * _Nonnull s);
extern int __ffill(FILE * _Nonnull s);
extern int __fflush(FILE* _Nonnull s);
extern int __setvbuf(FILE * _Restrict s, char * _Restrict buffer, int mode, size_t size);
extern int __fclose(FILE* _Nonnull s);

extern char * _Nullable __tmpnam_r(char * _Nullable _Restrict filename, int* _Nonnull _Restrict pOutIoc);

__CPP_END

#endif /* __STDIO_H */

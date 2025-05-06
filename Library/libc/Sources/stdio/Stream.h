//
//  Stream.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Stream_h
#define Stream_h 1

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <__stddef.h>

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
    __kStreamDirection_None = 0,
    __kStreamDirection_Read,
    __kStreamDirection_Write
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
    int     ioc;
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


extern errno_t __fopen_parse_mode(const char* _Nonnull mode, __FILE_Mode* _Nonnull pOutMode);

extern errno_t __fopen_init(FILE* _Nonnull self, bool bFreeOnClose, void* context, const FILE_Callbacks* callbacks, __FILE_Mode sm);
extern errno_t __fdopen_init(__IOChannel_FILE* _Nonnull self, bool bFreeOnClose, int ioc, __FILE_Mode sm);
extern errno_t __fopen_filename_init(__IOChannel_FILE* _Nonnull self, bool bFreeOnClose, const char *filename, __FILE_Mode sm);
extern errno_t __fopen_memory_init(__Memory_FILE* _Nonnull self, bool bFreeOnClose, FILE_Memory *mem, __FILE_Mode sm);
extern errno_t __fopen_null_init(FILE* _Nonnull self, bool bFreeOnClose, __FILE_Mode sm);

extern FILE *__fopen_null(const char* mode);

extern int __fflush(FILE* _Nonnull s);
extern int __fclose(FILE* _Nonnull s);

extern void __init_open_files_lock(void);

typedef int (*__file_func_t)(FILE* _Nonnull s);
extern int __iterate_open_files(__file_func_t _Nonnull f);

__CPP_END

#endif /* Stream_h */

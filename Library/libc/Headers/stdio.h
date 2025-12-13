//
//  stdio.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDIO_H
#define _STDIO_H 1

#include <_cmndef.h>
#include <_mbstate.h>
#include <_null.h>
#include <_off.h>
#include <_size.h>
#include <_ssize.h>
#include <kpi/_seek.h>
#include <kpi/syslimits.h>
#include <stdarg.h>

__CPP_BEGIN

#define EOF -1
#define FOPEN_MAX 16
#define FILENAME_MAX __PATH_MAX
#define BUFSIZ  1024

#define P_tmpdir "/tmp"
#define L_tmpnam 256
#define TMP_MAX 0x7ffffffe

#define _IONBF  0
#define _IOLBF  1
#define _IOFBF  2


typedef struct fpos_t {
    long long   offset;
    mbstate_t   mbstate;
} fpos_t;


// A stream defined by a set of callbacks
typedef ssize_t (*FILE_Read)(void *self, void *buffer, ssize_t nBytesToRead);
typedef ssize_t (*FILE_Write)(void *self, const void *buffer, ssize_t nBytesToWrite);
typedef long long (*FILE_Seek)(void *self, long long offset, int whence);
typedef int (*FILE_Close)(void *self);

typedef struct FILE_Callbacks {
    FILE_Read _Nullable     read;
    FILE_Write _Nullable    write;
    FILE_Seek _Nullable     seek;
    FILE_Close _Nullable    close;
} FILE_Callbacks;


// A memory-backed stream
typedef struct FILE_Memory {
    void* _Nullable     base;               // the (initial) memory block. The block will be reallocated if necessary and the current capacity is < maximumCapacity
    size_t              initialEof;         // initial file size. A fwrite() issued to a stream right after opening in append mode will write data starting at this location
    size_t              initialCapacity;    // Capacity of the initial memory block. This is the size to which a file will grow before an attempt is made to allocate a bigger block
    size_t              maximumCapacity;    // max size to which the memory block is allowed to grow. If initialCapacity == maximumCapacity then the stream will not grow the memory block
    unsigned int        options;            // See _IOM_xxx definitions
} FILE_Memory;

// Free the file memory block when fclose() is called
#define _IOM_FREE_ON_CLOSE  1

typedef struct FILE_MemoryQuery {
    void* _Nullable     base;           // current memory block base pointer
    size_t              eof;            // offset to where the EOF is in the memory block (aka how much data was written)
    size_t              capacity;       // how big the memory block really is. Difference between capacity and EOF is storage not used by the file
} FILE_MemoryQuery;


typedef struct FILE {
    struct FILE* _Nullable      prev;
    struct FILE* _Nullable      next;
    FILE_Callbacks              cb;
    void* _Nullable             context;
    unsigned char* _Nullable    buffer;
    ssize_t                     bufferCapacity;
    ssize_t                     bufferCount;
    ssize_t                     bufferIndex;    // Index of next character to return from the buffer
    char                        ugb;
    signed char                 ugbCount;
    char                        reserved[2];
    mbstate_t                   mbstate;
    struct FILE_Flags {
        unsigned int mode:3;
        unsigned int direction:2;
        unsigned int orientation:2;
        unsigned int bufferMode:2;
        unsigned int bufferOwned:1;     // 1 -> buffer owned by stream; 0 -> buffer owned by user
        unsigned int hasError:1;
        unsigned int hasEof:1;
        unsigned int shouldFreeOnClose:1;
        unsigned int reserved:19;
    }                           flags;
} FILE;


extern FILE* _Nonnull _Stdin;
extern FILE* _Nonnull _Stdout;
extern FILE* _Nonnull _Stderr;
#define stdin _Stdin
#define stdout _Stdout
#define stderr _Stderr


extern FILE *fopen(const char * _Nonnull _Restrict filename, const char * _Nonnull _Restrict mode);
extern FILE *freopen(const char * _Nonnull _Restrict filename, const char * _Nonnull _Restrict mode, FILE * _Nonnull _Restrict s);

extern FILE *fdopen(int ioc, const char * _Nonnull mode);
extern FILE *fdreopen(int ioc, const char * _Nonnull _Restrict mode, FILE * _Nonnull _Restrict s);

extern FILE *fopen_callbacks(void* _Nullable _Restrict context, const FILE_Callbacks * _Nonnull _Restrict callbacks, const char* _Nonnull _Restrict mode);
extern FILE *fopen_memory(FILE_Memory * _Nonnull _Restrict mem, const char * _Nonnull _Restrict mode);

extern int fclose(FILE * _Nonnull s);

extern int fileno(FILE * _Nonnull s);
extern int filemem(FILE * _Nonnull _Restrict s, FILE_MemoryQuery * _Nonnull _Restrict query);

extern void setbuf(FILE * _Nonnull _Restrict s, char * _Nullable _Restrict buffer);
extern int setvbuf(FILE * _Nonnull _Restrict s, char * _Nullable _Restrict buffer, int mode, size_t size);

extern void clearerr(FILE * _Nonnull s);
extern int feof(FILE * _Nonnull s);
extern int ferror(FILE * _Nonnull s);

extern off_t ftello(FILE * _Nonnull s);
extern long ftell(FILE * _Nonnull s);

extern int fseeko(FILE * _Nonnull s, off_t offset, int whence);
extern int fseek(FILE * _Nonnull s, long offset, int whence);

extern int fgetpos(FILE * _Nonnull _Restrict s, fpos_t * _Nonnull _Restrict pos);
extern int fsetpos(FILE * _Nonnull _Restrict s, const fpos_t * _Nonnull _Restrict pos);

extern void rewind(FILE * _Nonnull s);

extern ssize_t getline(char **line, size_t * _Nonnull _Restrict n, FILE * _Nonnull _Restrict s);
extern ssize_t getdelim(char **line, size_t * _Nonnull _Restrict n, int delimiter, FILE * _Nonnull _Restrict s);

extern int fgetc(FILE * _Nonnull s);
#define getc(s) fgetc(s)

extern char * _Nonnull fgets(char * _Nonnull _Restrict str, int count, FILE * _Nonnull _Restrict s);

extern int fputc(int ch, FILE * _Nonnull s);
#define putc(ch, s) fputc(ch, s)

extern int fputs(const char * _Nonnull _Restrict str, FILE * _Nonnull _Restrict s);
extern int ungetc(int ch, FILE * _Nonnull s); // supports one character push back

extern size_t fread(void * _Nonnull _Restrict buffer, size_t size, size_t count, FILE * _Nonnull _Restrict s);
extern size_t fwrite(const void * _Nonnull _Restrict buffer, size_t size, size_t count, FILE * _Nonnull _Restrict s);

extern int fflush(FILE * _Nonnull s);

extern int getchar(void);
extern char * _Nullable gets(char * _Nonnull str);

extern int putchar(int ch);
extern int puts(const char * _Nonnull str);

extern int printf(const char * _Nonnull _Restrict format, ...);
extern int vprintf(const char * _Nonnull _Restrict format, va_list ap);

extern int fprintf(FILE * _Nonnull _Restrict s, const char * _Nonnull _Restrict format, ...);
extern int vfprintf(FILE * _Nonnull _Restrict s, const char * _Nonnull _Restrict format, va_list ap);

extern int sprintf(char * _Nullable _Restrict buffer, const char * _Nonnull _Restrict format, ...);
extern int vsprintf(char * _Nullable _Restrict buffer, const char * _Nonnull _Restrict format, va_list ap);
extern int snprintf(char * _Nullable _Restrict buffer, size_t bufsiz, const char * _Nonnull _Restrict format, ...);
extern int vsnprintf(char * _Nullable _Restrict buffer, size_t bufsiz, const char * _Nonnull _Restrict format, va_list ap);

extern int asprintf(char **str_ptr, const char * _Nonnull _Restrict format, ...);
extern int vasprintf(char **str_ptr, const char * _Nonnull _Restrict format, va_list ap);

extern int scanf(const char * _Nonnull _Restrict format, ...);
extern int vscanf(const char * _Nonnull _Restrict format, va_list ap);

extern int fscanf(FILE * _Nonnull _Restrict s, const char * _Nonnull _Restrict format, ...);
extern int vfscanf(FILE * _Nonnull _Restrict s, const char * _Nonnull _Restrict format, va_list ap);

extern int sscanf(const char * _Nonnull _Restrict buffer, const char * _Nonnull _Restrict format, ...);
extern int vsscanf(const char * _Nonnull _Restrict buffer, const char * _Nonnull _Restrict format, va_list ap);

extern void perror(const char * _Nullable str);

extern int remove(const char* _Nonnull path);
extern int rename(const char* _Nonnull oldpath, const char* _Nonnull newpath);

extern char * _Nullable tmpnam(char * _Nonnull filename);
extern char * _Nullable tmpnam_r(char * _Nonnull filename);

extern FILE * _Nullable tmpfile(void);

__CPP_END

#endif /* _STDIO_H */

//
//  stdio.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDIO_H
#define _STDIO_H 1

#include <kern/_cmndef.h>
#include <kern/_null.h>
#include <machine/abi/_size.h>
#include <machine/abi/_ssize.h>
#include <kern/_syslimits.h>
#include <kern/_errno.h>
#include <stdarg.h>

__CPP_BEGIN

#define EOF -1
#define FOPEN_MAX 16
#define FILENAME_MAX __PATH_MAX
#define BUFSIZ  4096

#define P_tmpdir "/tmp"
#define L_tmpnam 256
#define TMP_MAX 0x7ffffffe

#define _IONBF  0
#define _IOLBF  1
#define _IOFBF  2

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2


typedef struct fpos_t {
    long long   offset;
} fpos_t;


// A stream defined by a set of callbacks
typedef _Errno_t (*FILE_Read)(void *self, void *buffer, ssize_t nBytesToRead, ssize_t *pOutBytesRead);
typedef _Errno_t (*FILE_Write)(void *self, const void *buffer, ssize_t nBytesToWrite, ssize_t *pOutBytesWritten);
typedef _Errno_t (*FILE_Seek)(void *self, long long offset, long long *outOldOffset, int whence);
typedef _Errno_t (*FILE_Close)(void *self);

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
    char* _Nullable             buffer;
    size_t                      bufferCapacity;
    size_t                      bufferCount;
    struct FILE_Flags {
        unsigned int mode:3;
        unsigned int mostRecentDirection:2;
        unsigned int hasError:1;
        unsigned int hasEof:1;
        unsigned int shouldFreeOnClose:1;
        unsigned int reserved:24;
    }                           flags;
} FILE;


extern FILE* _Nonnull _Stdin;
extern FILE* _Nonnull _Stdout;
extern FILE* _Nonnull _Stderr;
#define stdin _Stdin
#define stdout _Stdout
#define stderr _Stderr


extern FILE *fopen(const char *filename, const char *mode);
extern FILE *freopen(const char *filename, const char *mode, FILE *s);

extern FILE *fdopen(int ioc, const char *mode);
extern FILE *fdreopen(int ioc, const char *mode, FILE *s);

extern FILE *fopen_callbacks(void* context, const FILE_Callbacks *callbacks, const char* mode);
extern FILE *fopen_memory(FILE_Memory *mem, const char *mode);

extern int fclose(FILE *s);

extern int fileno(FILE *s);
extern int filemem(FILE *s, FILE_MemoryQuery *query);

extern void setbuf(FILE *s, char *buffer);
extern int setvbuf(FILE *s, char *buffer, int mode, size_t size);

extern void clearerr(FILE *s);
extern int feof(FILE *s);
extern int ferror(FILE *s);

extern long ftell(FILE *s);
extern int fseek(FILE *s, long offset, int whence);
extern int fgetpos(FILE *s, fpos_t *pos);
extern int fsetpos(FILE *s, const fpos_t *pos);
extern void rewind(FILE *s);

extern ssize_t getline(char **line, size_t *n, FILE *s);
extern ssize_t getdelim(char **line, size_t *n, int delimiter, FILE *s);

extern int fgetc(FILE *s);
#define getc(s) fgetc(s)

extern char *fgets(char *str, int count, FILE *s);

extern int fputc(int ch, FILE *s);
#define putc(ch, s) fputc(ch, s)

extern int fputs(const char *str, FILE *s);
extern int ungetc(int ch, FILE *s);

extern size_t fread(void *buffer, size_t size, size_t count, FILE *s);
extern size_t fwrite(const void *buffer, size_t size, size_t count, FILE *s);

extern int fflush(FILE *s);

extern int getchar(void);
extern char *gets(char *str);

extern int putchar(int ch);
extern int puts(const char *str);

extern int printf(const char *format, ...);
extern int vprintf(const char *format, va_list ap);

extern int fprintf(FILE *s, const char *format, ...);
extern int vfprintf(FILE *s, const char *format, va_list ap);

extern int sprintf(char *buffer, const char *format, ...);
extern int vsprintf(char *buffer, const char *format, va_list ap);
extern int snprintf(char *buffer, size_t bufsiz, const char *format, ...);
extern int vsnprintf(char *buffer, size_t bufsiz, const char *format, va_list ap);

extern int asprintf(char **str_ptr, const char *format, ...);
extern int vasprintf(char **str_ptr, const char *format, va_list ap);

extern int scanf(const char *format, ...);
extern int vscanf(const char *format, va_list ap);

extern int fscanf(FILE *s, const char *format, ...);
extern int vfscanf(FILE *s, const char *format, va_list ap);

extern int sscanf(const char *buffer, const char *format, ...);
extern int vsscanf(const char *buffer, const char *format, va_list ap);

extern void perror(const char *str);

extern int remove(const char* path);
extern int rename(const char* oldpath, const char* newpath);

extern char *tmpnam(char *filename);
extern char *tmpnam_r(char *filename);

extern FILE *tmpfile(void);

__CPP_END

#endif /* _STDIO_H */

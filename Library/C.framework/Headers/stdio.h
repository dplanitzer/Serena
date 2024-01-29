//
//  stdio.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDIO_H
#define _STDIO_H 1

#include <_cmndef.h>
#include <_errdef.h>
#include <_nulldef.h>
#include <_sizedef.h>
#include <_syslimits.h>
#include <stdarg.h>

__CPP_BEGIN

#define EOF -1
#define FOPEN_MAX 16
#define FILENAME_MAX __PATH_MAX
#define BUFSIZ  4096

#define _IONBF  0
#define _IOLBF  1
#define _IOFBF  2

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2


typedef struct fpos_t {
    long long   offset;
} fpos_t;

typedef long __FILE_Mode;

enum {
    __kStreamMode_Read = 0x01,      // Allow reading
    __kStreamMode_Write = 0x02,     // Allow writing
    __kStreamMode_Append = 0x04,    // Append to the file
    __kStreamMode_Exclusive = 0x08  // Fail if file already exists instead of creating it
};

enum {
    __kStreamDirection_None = 0,
    __kStreamDirection_Read,
    __kStreamDirection_Write
};

typedef __errno_t (*__FILE_read)(void* self, void* pBuffer, __ssize_t nBytesToRead, __ssize_t* pOutBytesRead);
typedef __errno_t (*__FILE_write)(void* self, const void* pBytes, __ssize_t nBytesToWrite, __ssize_t* pOutBytesWritten);
typedef __errno_t (*__FILE_seek)(void* self, long long offset, long long *outOldOffset, int whence);
typedef __errno_t (*__FILE_close)(void* self);

struct __FILE_funopen {
    void *  context;
};

struct __FILE_fdopen {
    int fd;
};

typedef struct __FILE {
    struct __FILE*              prev;
    struct __FILE*              next;
    __FILE_read _Nullable       readfn;
    __FILE_write _Nullable      writefn;
    __FILE_seek _Nullable       seekfn;
    __FILE_close _Nullable      closefn;
    union __FILE_Stream {
        struct __FILE_fdopen        ioc;
        struct __FILE_funopen       callbacks;
    }                           u;
    char*                       buffer;
    size_t                      bufferCapacity;
    size_t                      bufferCount;
    struct __FILE_Flags {
        unsigned int mode:3;
        unsigned int mostRecentDirection:2;
        unsigned int hasError:1;
        unsigned int hasEof:1;
    }                           flags;
    long                        reserved[4];
} FILE;

extern FILE __StdFile[3];
#define stdin &__StdFile[0]
#define stdout &__StdFile[1]
#define stderr &__StdFile[2]


extern FILE *fopen(const char *filename, const char *mode);
extern FILE *fdopen(int ioc, const char *mode);
extern FILE *funopen(void* context, __FILE_read readfn, __FILE_write writefn, __FILE_seek seekfn, __FILE_close closefn);
extern int fclose(FILE *s);

extern int fileno(FILE *s);

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

extern int sprintf(char *buffer, const char *format, ...);
extern int vsprintf(char *buffer, const char *format, va_list ap);
extern int snprintf(char *buffer, size_t bufsiz, const char *format, ...);
extern int vsnprintf(char *buffer, size_t bufsiz, const char *format, va_list ap);

extern int asprintf(char **str_ptr, const char *format, ...);
extern int vasprintf(char **str_ptr, const char *format, va_list ap);

extern int scanf(const char *format, ...);
extern int vscanf(const char *format, va_list ap);

extern int sscanf(const char *buffer, const char *format, ...);
extern int vsscanf(const char *buffer, const char *format, va_list ap);

extern void perror(const char *str);

extern int remove(const char* path);
extern int rename(const char* oldpath, const char* newpath);

__CPP_END

#endif /* _STDIO_H */

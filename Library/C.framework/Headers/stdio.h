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
#include <stdint.h>

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


typedef __errno_t (*FILE_Read)(void* self, void* pBuffer, __ssize_t nBytesToRead, __ssize_t* pOutBytesRead);
typedef __errno_t (*FILE_Write)(void* self, const void* pBytes, __ssize_t nBytesToWrite, __ssize_t* pOutBytesWritten);
typedef __errno_t (*FILE_Seek)(void* self, long long offset, long long *outOldOffset, int whence);
typedef __errno_t (*FILE_Close)(void* self);

typedef struct FILE_Callbacks {
    FILE_Read _Nullable     read;
    FILE_Write _Nullable    write;
    FILE_Seek _Nullable     seek;
    FILE_Close _Nullable    close;
} FILE_Callbacks;


typedef struct _FILE {
    struct _FILE*               prev;
    struct _FILE*               next;
    FILE_Callbacks              cb;
    intptr_t                    context;
    char*                       buffer;
    size_t                      bufferCapacity;
    size_t                      bufferCount;
    struct _FILE_Flags {
        unsigned int mode:3;
        unsigned int mostRecentDirection:2;
        unsigned int hasError:1;
        unsigned int hasEof:1;
        unsigned int shouldFreeOnClose:1;
    }                           flags;
    long                        reserved[4];
} FILE;

extern FILE __StdFile[3];
#define stdin (&__StdFile[0])
#define stdout (&__StdFile[1])
#define stderr (&__StdFile[2])


extern FILE *fopen(const char *filename, const char *mode);
extern FILE *fdopen(int ioc, const char *mode);
extern FILE *fopen_callbacks(void* context, const FILE_Callbacks* callbacks, const char* mode);
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

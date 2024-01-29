//
//  Stream.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <limits.h>
#include <stdlib.h>
#include <apollo/apollo.h>


static FILE*   gOpenFiles;


static errno_t ioc_read(struct __FILE_fdopen* self, void* pBuffer, ssize_t nBytesToRead, ssize_t* pOutBytesRead)
{
    ssize_t bytesRead = read(self->fd, pBuffer, nBytesToRead);

    if (bytesRead >= 0) {
        *pOutBytesRead = bytesRead;
        return 0;
    } else {
        *pOutBytesRead = 0;
        return (errno_t) -bytesRead;
    }
}

static errno_t ioc_write(struct __FILE_fdopen* self, const void* pBytes, ssize_t nBytesToWrite, ssize_t* pOutBytesWritten)
{
    ssize_t bytesWritten = write(self->fd, pBytes, nBytesToWrite);

    if (bytesWritten >= 0) {
        *pOutBytesWritten = bytesWritten;
        return 0;
    } else {
        *pOutBytesWritten = 0;
        return (errno_t) -bytesWritten;
    }
}

static errno_t ioc_seek(struct __FILE_fdopen* self, long long offset, long long *outOldOffset, int whence)
{
    return seek(self->fd, (off_t)offset, outOldOffset, whence);
}

static errno_t ioc_close(struct __FILE_fdopen* self)
{
    return close(self->fd);
}


// Parses the given mode string into a stream-mode value
static __FILE_Mode parse_mode(const char* _Nonnull mode)
{
    __FILE_Mode sm = 0;

    while (*mode != '\0') {
        if (*mode == 'r') {
            sm |= __kStreamMode_Read;

            if (*(mode + 1) == '+') {
                sm |= __kStreamMode_Write;
                mode++;
            }
        }
        else if (*mode == 'w') {
            sm |= __kStreamMode_Write;

            if (*(mode + 1) == '+') {
                sm |= __kStreamMode_Read;
                mode++;
            }
            if (*(mode + 1) == 'x') {
                sm |= __kStreamMode_Exclusive;
                mode++;
            }
        }
        else if (*mode == 'a') {
            sm |= (__kStreamMode_Append | __kStreamMode_Write);

            if (*(mode + 1) == '+') {
                sm |= __kStreamMode_Read;
                mode++;
            }
        }

        mode++;
    }

    return sm;
}

errno_t __fopen_init(FILE* self, __FILE_read readfn, __FILE_write writefn, __FILE_seek seekfn, __FILE_close closefn, __FILE_Mode mode)
{
    decl_try_err();

    if (mode == 0) {
        return EINVAL;
    }

    self->flags.mode = mode;
    self->flags.mostRecentDirection = __kStreamDirection_None;
    self->readfn = readfn;
    self->writefn = writefn;
    self->seekfn = seekfn;
    self->closefn = closefn;

    if (gOpenFiles) {
        self->next = gOpenFiles;
        gOpenFiles->prev = self;
        gOpenFiles = self;
    } else {
        gOpenFiles = self;
    }

    return 0;
}

FILE *__fopen(__FILE_read readfn, __FILE_write writefn, __FILE_seek seekfn, __FILE_close closefn, __FILE_Mode mode)
{
    decl_try_err();
    FILE* self = NULL;

    try_null(self, calloc(1, sizeof(FILE)), ENOMEM);
    try(__fopen_init(self, readfn, writefn, seekfn, closefn, mode));
    return self;

catch:
    free(self);
    errno = err;
    return NULL;
}

FILE *funopen(void* context, __FILE_read readfn, __FILE_write writefn, __FILE_seek seekfn, __FILE_close closefn)
{
    decl_try_err();
    FILE* self = NULL;
    int sm = 0;

    if (readfn) {
        sm |= __kStreamMode_Read;
    }
    if (writefn) {
        sm |= __kStreamMode_Write;
    }
    if (sm == 0) {
        throw(EINVAL);
    }

    try_null(self, __fopen(readfn, writefn, seekfn, closefn, sm), ENOMEM);
    self->u.callbacks.context = context;
    return self;

catch:
    errno = err;
    return NULL;
}

errno_t __fdopen_init(FILE* self, int ioc, const char *mode)
{
    decl_try_err();
    const int sm = parse_mode(mode);

    if (sm == 0) {
        throw(EINVAL);
    }

    try(__fopen_init(self, (__FILE_read)ioc_read, (__FILE_write)ioc_write, (__FILE_seek)ioc_seek, (__FILE_close)ioc_close, sm));
    self->u.ioc.fd = ioc;
    return 0;

catch:
    errno = err;
    return err;
}

FILE *fdopen(int ioc, const char *mode)
{
    decl_try_err();
    FILE* self = NULL;
    const int sm = parse_mode(mode);

    if (sm == 0) {
        throw(EINVAL);
    }

    try_null(self, __fopen((__FILE_read)ioc_read, (__FILE_write)ioc_write, (__FILE_seek)ioc_seek, (__FILE_close)ioc_close, sm), ENOMEM);
    self->u.ioc.fd = ioc;

    return self;

catch:
    errno = err;
    return NULL;
}

FILE *fopen(const char *filename, const char *mode)
{
    decl_try_err();
    FILE* self = NULL;
    const int sm = parse_mode(mode);
    int fd = -1;

    int options = 0;
    if ((sm & __kStreamMode_Read) != 0) {
        options |= O_RDONLY;
    }
    if ((sm & __kStreamMode_Write) != 0) {
        options |= O_WRONLY;
        if ((sm & __kStreamMode_Append) != 0) {
            options |= O_APPEND;
        } else {
            options |= O_TRUNC;
        }
        if ((sm & __kStreamMode_Exclusive) != 0) {
            options |= O_EXCL;
        }
    }

    if (options == 0) {
        throw(EINVAL);
    }


    // Open/create the file
    if ((sm & __kStreamMode_Write) == 0) {
        // Read only
        try(open(filename, options, &fd));
    }
    else {
        // Write only or read/write/append
        try(creat(filename, options, 0666, &fd));
    }
    
    try_null(self, __fopen((__FILE_read)ioc_read, (__FILE_write)ioc_write, (__FILE_seek)ioc_seek, (__FILE_close)ioc_close, sm), ENOMEM);
    self->u.ioc.fd = fd;


    // Make sure that the return value of ftell() lines up with the actual
    // end of file position.
    if ((sm & __kStreamMode_Append) != 0) {
        self->seekfn(&self->u, 0ll, NULL, SEEK_END);
    }

    return self;

catch:
    if (fd != -1) {
        close(fd);
    }
    errno = err;
    return NULL;
}

int fclose(FILE *s)
{
    decl_try_err();

    if (s) {
        err = fflush(s);
        const errno_t err2 = s->closefn(&s->u);
        if (err == 0) { err = err2; }

        if (gOpenFiles == s) {
            (s->next)->prev = NULL;
            gOpenFiles = s->next;
        } else {
            (s->next)->prev = s->prev;
            (s->prev)->next = s->next;
        }
        s->prev = NULL;
        s->next = NULL;

        free(s);
    }
    return err;
}

int fileno(FILE *s)
{
    return (s->readfn == (__FILE_read)ioc_read) ? s->u.ioc.fd : EOF;
}

void setbuf(FILE *s, char *buffer)
{
    if (buffer) {
        (void) setvbuf(s, buffer, _IOFBF, BUFSIZ);
    } else {
        (void) setvbuf(s, NULL, _IONBF, 0);
    }
}

int setvbuf(FILE *s, char *buffer, int mode, size_t size)
{
    // XXX
    return EOF;
}

void clearerr(FILE *s)
{
    s->flags.hasError = 0;
    s->flags.hasEof = 0;
}

int feof(FILE *s)
{
    return (s->flags.hasEof) ? EOF : 0;
}

int ferror(FILE *s)
{
    return (s->flags.hasError) ? EOF : 0;
}

long ftell(FILE *s)
{
    decl_try_err();
    long long curpos;

    if (s->seekfn == NULL) {
        errno = ESPIPE;
        return (long)EOF;
    }

    err = s->seekfn(&s->u, 0ll, &curpos, SEEK_CUR);
#if __LONG_WIDTH == 64
    return (err == 0) (long)curpos : (long)EOF;
#else
    return (err == 0 && curpos <= (long long)LONG_MAX) ? (long)curpos : (long)EOF;
#endif
}

int fseek(FILE *s, long offset, int whence)
{
    decl_try_err();

    if (s->seekfn == NULL) {
        errno = ESPIPE;
        return EOF;
    }
    switch (whence) {
        case SEEK_SET:
        case SEEK_CUR:
        case SEEK_END:
            break;

        default:
            errno = EINVAL;
            return EOF;
    }

    if (s->flags.mostRecentDirection == __kStreamDirection_Write) {
        const int e = fflush(s);
        if (e != 0) {
            return EOF;
        }
    }

    err = s->seekfn(&s->u, (long long)offset, NULL, whence);
    if (err != 0) {
        s->flags.hasError = 1;
        return EOF;
    }
    if (!(offset == 0ll && whence == SEEK_CUR)) {
        s->flags.hasEof = 0;
    }
    // XXX drop ungetc buffered stuff

    return 0;
}

int fgetpos(FILE *s, fpos_t *pos)
{
    decl_try_err();

    if (s->seekfn == NULL) {
        errno = ESPIPE;
        return EOF;
    }

    err = s->seekfn(&s->u, 0ll, &pos->offset, SEEK_CUR);
    return (err == 0) ? 0 : EOF;
}

int fsetpos(FILE *s, const fpos_t *pos)
{
    decl_try_err();

    if (s->seekfn == NULL) {
        errno = ESPIPE;
        return EOF;
    }

    if (s->flags.mostRecentDirection == __kStreamDirection_Write) {
        const int e = fflush(s);
        if (e != 0) {
            return EOF;
        }
    }

    err = s->seekfn(&s->u, pos->offset, NULL, SEEK_SET);
    if (err != 0) {
        s->flags.hasError = 1;
        return EOF;
    }
    s->flags.hasEof = 0;
    // XXX drop ungetc buffered stuff

    return 0;
}

void rewind(FILE *s)
{
    (void) fseek(s, 0, SEEK_SET);
    clearerr(s);
    // XXX drop ungetc buffered stuff
}

int fgetc(FILE *s)
{
    if ((s->flags.mode & __kStreamMode_Read) == 0) {
        s->flags.hasError = 1;
        return EOF;
    }

    char buf;
    ssize_t nBytesRead;
    const errno_t r = s->readfn(&s->u, &buf, 1, &nBytesRead);

    if (r == 0) {
        if (nBytesRead == 1) {
            s->flags.hasEof = 0;
            return (int)(unsigned char)buf;
        } else {
            s->flags.hasEof = 1;
        }
    } else {
        s->flags.hasError = 1;
    }

    return EOF;
}

char *fgets(char *str, int count, FILE *s)
{
    int err = 0;
    int nBytesToRead = count - 1;
    int nBytesRead = 0;

    if (count < 1) {
        return NULL;
    }

    while (nBytesToRead-- > 0) {
        const int ch = fgetc(s);

        if (ch == EOF) {
            break;
        }

        str[nBytesRead++] = ch;

        if (ch == '\n') {
            break;
        }
    }
    str[nBytesRead] = '\0';

    return (s->flags.hasError || (s->flags.hasEof && nBytesRead == 0)) ? NULL : str;
}

int fputc(int ch, FILE *s)
{
    if ((s->flags.mode & __kStreamMode_Write) == 0) {
        s->flags.hasError = 1;
        return EOF;
    }

    unsigned char buf = (unsigned char)ch;
    ssize_t nBytesWritten;
    const errno_t r = s->writefn(&s->u, &buf, 1, &nBytesWritten);

    if (r == 0) {
        if (nBytesWritten == 1) {
            s->flags.hasEof = 0;
            return (int)buf;
        } else {
            s->flags.hasEof = 1;
        }
    } else {
        s->flags.hasError = 1;
    }

    return EOF;
}

int fputs(const char *str, FILE *s)
{
    int err = 0;

    while (*str != '\0' && err == 0) {
        err = fputc(*str++, s);
    }
    return err;
}

int ungetc(int ch, FILE *s)
{
    // XXX
    return EOF;
}

size_t fread(void *buffer, size_t size, size_t count, FILE *s)
{
    if (size == 0 || count == 0) {
        return 0;
    }

    size_t nBytesToRead = size * count;
    size_t nBytesRead = 0;
    while (nBytesToRead-- > 0) {
        const int ch = fgetc(s);

        if (ch == EOF) {
            break;
        }

        ((unsigned char*)buffer)[nBytesRead++] = (unsigned char)ch;
    }

    return nBytesRead / size;
}

size_t fwrite(const void *buffer, size_t size, size_t count, FILE *s)
{
    if (size == 0 || count == 0) {
        return 0;
    }

    size_t nBytesToWrite = size * count;
    size_t nBytesWritten = 0;
    while (nBytesToWrite-- > 0) {
        const int r = fputc(((const unsigned char*)buffer)[nBytesWritten++], s);

        if (r == EOF) {
            break;
        }
    }

    return nBytesWritten / size;
}

#if 0
//XXX general approach of bulk writing
static errno_t __write(const char* _Nonnull pBuffer, size_t nBytes)
{
    ssize_t nBytesWritten = 0;

    while (nBytesWritten < nBytes) {
        const ssize_t r = write(STDOUT_FILENO, pBuffer, nBytes);
        
        if (r < 0) {
            return (errno_t) -r;
        }
        nBytesWritten += nBytes;
    }

    return 0;
}
#endif

int fflush(FILE *s)
{
    decl_try_err();

    if (s) {
        // XXX
    }
    else {
        FILE* pCurFile = gOpenFiles;
        
        while (pCurFile) {
            if (pCurFile->flags.mostRecentDirection == __kStreamDirection_Write) {
                const errno_t e = fflush(pCurFile);

                if (err == 0) {
                    err = e;
                }
            }
            pCurFile = pCurFile->next;
        }
    }
    return err;
}

int getchar(void)
{
    return getc(stdin);
}

char *gets(char *str)
{
    if (str == NULL) {
        return NULL;
    }

    char* p = str;

    while (true) {
        const int ch = getchar();

        if (ch == EOF || ch == '\n') {
            break;
        }

        *p++ = (char)ch;
    }
    *p = '\0';
    
    return str;
}

int putchar(int ch)
{
    return putc(ch, stdout);
}

int puts(const char *str)
{
    const int r = fputs(str, stdout);

    return (r == 0) ? fputc('\n', stdout) : r;
}

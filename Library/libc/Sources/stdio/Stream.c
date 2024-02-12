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
#include <string.h>
#include <apollo/apollo.h>


static FILE*   gOpenFiles;


// Parses the given mode string into a stream-mode value
__FILE_Mode __fopen_parse_mode(const char* _Nonnull mode)
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

errno_t __fopen_init(FILE* _Nonnull self, bool bFreeOnClose, void* context, const FILE_Callbacks* callbacks, const char* mode)
{
    if (callbacks == NULL || mode == NULL) {
        return EINVAL;
    }

    __FILE_Mode sm = __fopen_parse_mode(mode);

    if (sm & (__kStreamMode_Read|__kStreamMode_Write) == 0) {
        return EINVAL;
    }
    if ((sm & __kStreamMode_Read) != 0 && callbacks->read == NULL) {
        return EINVAL;
    }
    if ((sm & __kStreamMode_Write) != 0 && callbacks->write == NULL) {
        return EINVAL;
    }

    memset(self, 0, sizeof(FILE));
    self->cb = *callbacks;
    self->context = context;
    self->flags.mode = sm;
    self->flags.mostRecentDirection = __kStreamDirection_None;
    self->flags.shouldFreeOnClose = bFreeOnClose;

    if (gOpenFiles) {
        self->next = gOpenFiles;
        gOpenFiles->prev = self;
        gOpenFiles = self;
    } else {
        gOpenFiles = self;
    }

    return 0;
}

FILE *fopen_callbacks(void* context, const FILE_Callbacks* callbacks, const char* mode)
{
    decl_try_err();
    FILE* self = NULL;

    try_null(self, malloc(SIZE_OF_FILE_SUBCLASS(FILE)), ENOMEM);
    try(__fopen_init(self, true, context, callbacks, mode));
    return self;

catch:
    free(self);
    errno = err;
    return NULL;
}

// Shuts down the given stream but does not free the 's' memory block. 
int __fclose(FILE * _Nonnull s)
{
    int r = fflush(s);
        
    const errno_t err = (s->cb.close) ? s->cb.close((void*)s->context) : 0;
    if (r == 0 && err != 0) {
        errno = err;
        r = EOF;
    }

    if (gOpenFiles == s) {
        (s->next)->prev = NULL;
        gOpenFiles = s->next;
    } else {
        (s->next)->prev = s->prev;
        (s->prev)->next = s->next;
    }
    s->prev = NULL;
    s->next = NULL;
    
    return r;
}

int fclose(FILE *s)
{
    int r = 0;

    if (s) {
        r = __fclose(s);

        if (s->flags.shouldFreeOnClose) {
            free(s);
        }
    }
    return r;
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
    long long curpos;

    if (s->cb.seek == NULL) {
        errno = ESPIPE;
        return (long)EOF;
    }

    const errno_t err = s->cb.seek((void*)s->context, 0ll, &curpos, SEEK_CUR);
    if (err != 0) {
        s->flags.hasError = 1;
        errno = err;
        return (long)EOF;
    }

#if __LONG_WIDTH == 64
    return (long)curpos;
#else
    if (curpos > (long long)LONG_MAX) {
        errno = ERANGE;
        return EOF;
    } else {
        return (long)curpos;
    }
#endif
}

int fseek(FILE *s, long offset, int whence)
{
    if (s->cb.seek == NULL) {
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

    const errno_t err = s->cb.seek((void*)s->context, (long long)offset, NULL, whence);
    if (err != 0) {
        s->flags.hasError = 1;
        errno = err;
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
    if (s->cb.seek == NULL) {
        errno = ESPIPE;
        return EOF;
    }

    const errno_t err = s->cb.seek((void*)s->context, 0ll, &pos->offset, SEEK_CUR);
    if (err != 0) {
        s->flags.hasError = 1;
        errno = err;
        return EOF;
    }

    return 0;
}

int fsetpos(FILE *s, const fpos_t *pos)
{
    if (s->cb.seek == NULL) {
        errno = ESPIPE;
        return EOF;
    }

    if (s->flags.mostRecentDirection == __kStreamDirection_Write) {
        const int e = fflush(s);
        if (e != 0) {
            return EOF;
        }
    }

    const errno_t err = s->cb.seek((void*)s->context, pos->offset, NULL, SEEK_SET);
    if (err != 0) {
        s->flags.hasError = 1;
        errno = err;
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
        errno = EBADF;
        return EOF;
    }

    int r;
    char buf;
    ssize_t nBytesRead;
    const errno_t err = s->cb.read((void*)s->context, &buf, 1, &nBytesRead);

    if (err == 0) {
        if (nBytesRead == 1) {
            s->flags.hasEof = 0;
            r = (int)(unsigned char)buf;
        } else {
            s->flags.hasEof = 1;
            r = EOF;
        }
    } else {
        s->flags.hasError = 1;
        errno = err;
        r = EOF;
    }

    return r;
}

char *fgets(char *str, int count, FILE *s)
{
    int nBytesToRead = count - 1;
    int nBytesRead = 0;

    if (count < 1) {
        errno = EINVAL;
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
        errno = EBADF;
        return EOF;
    }

    int r;
    unsigned char buf = (unsigned char)ch;
    ssize_t nBytesWritten;
    const errno_t err = s->cb.write((void*)s->context, &buf, 1, &nBytesWritten);

    if (err == 0) {
        if (nBytesWritten == 1) {
            s->flags.hasEof = 0;
            r = (int)buf;
        } else {
            s->flags.hasEof = 1;
            r = EOF;
        }
    } else {
        s->flags.hasError = 1;
        errno = err;
        r = EOF;
    }

    return r;
}

int fputs(const char *str, FILE *s)
{
    int nCharsWritten = 0;

    while (*str != '\0') {
        const int r = fputc(*str++, s);

        if (r == EOF) {
            return EOF;
        }

        if (nCharsWritten < INT_MAX) {
            nCharsWritten++;
        }
    }
    return nCharsWritten;
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
static errno_t __IOChannel_Write(const char* _Nonnull pBuffer, size_t nBytes)
{
    ssize_t nBytesWritten = 0;

    while (nBytesWritten < nBytes) {
        const ssize_t r = IOChannel_Write(kIOChannel_Stdout, pBuffer, nBytes);
        
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
    int r = 0;

    if (s) {
        // XXX
    }
    else {
        FILE* pCurFile = gOpenFiles;
        
        while (pCurFile) {
            if (pCurFile->flags.mostRecentDirection == __kStreamDirection_Write) {
                const int rx = fflush(pCurFile);

                if (r == 0) {
                    r = rx;
                }
            }
            pCurFile = pCurFile->next;
        }
    }
    return r;
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
    const int nCharsWritten = fputs(str, stdout);
    
    if (nCharsWritten >= 0) {
        const int r = fputc('\n', stdout);
        
        if (r != EOF) {
            return (nCharsWritten < INT_MAX) ? nCharsWritten + 1 : INT_MAX;
        }
    }
    return EOF;
}

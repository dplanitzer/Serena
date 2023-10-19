//
//  stdio.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <apollo/apollo.h>
#include <__stddef.h>


FILE __StdFile[3];


FILE *fopen(const char *filename, const char *mode)
{
    // XXX
    return NULL;
}

int fclose(FILE *s)
{
    // XXX
    return EOF;
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
    // XXX
}

int feof(FILE *s)
{
    // XXX
    return EOF;
}

int ferror(FILE *s)
{
    // XXX
    return EOF;
}

long ftell(FILE *s)
{
    // XXX
    return -1l;
}

int fseek(FILE *s, long offset, int origin)
{
    return EOF;
}

int fgetpos(FILE *s, fpos_t *pos)
{
    // XXX
    return EOF;
}

int fsetpos(FILE *s, const fpos_t *pos)
{
    // XXX
    return EOF;
}

void rewind(FILE *s)
{
    (void) fseek(s, 0, SEEK_SET);
    // XXX
    // clear errors & eof
    // drop ungetc buffered stuff
}

int fgetc(FILE *s)
{
    // XXX
    return EOF;
}

char *fgets(char *str, int count, FILE *s)
{
    // XXX
    return NULL;
}

int fputc(int ch, FILE *s)
{
    // XXX
    return EOF;
}

int fputs(const char *str, FILE *s)
{
    // XXX
    return EOF;
}

int ungetc(int ch, FILE *s)
{
    // XXX
    return EOF;
}

size_t fread(void *buffer, size_t size, size_t count, FILE *s)
{
    // XXX
    return 0;
}

size_t fwrite(const void *buffer, size_t size, size_t count, FILE *s)
{
    // XXX
    return 0;
}

int fflush(FILE *s)
{
    // XXX
    return EOF;
}

int getchar(void)
{
    char buf;
    const ssize_t r = read(STDIN_FILENO, &buf, 1);

    if (r < 0) {
        return EOF;
    } else {
        return (int)(unsigned char)buf;
    }
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
    if (ch == EOF) {
        return EOF;
    }

    unsigned char uch = (unsigned char) ch;

    if (__write((const char*)&uch, 1) == 0) {
        return uch;
    } else {
        return 0;
    }
}

int puts(const char *str)
{
    if (__write(str, strlen(str)) != 0) {
        return EOF;
    }

    return putchar('\n');
}

errno_t __write(const char* _Nonnull pBuffer, size_t nBytes)
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

void perror(const char *str)
{
    if (str && *str != '\0') {
        puts(str);
        puts(": ");
    }

    puts(strerror(errno));
    putchar('\n');
}

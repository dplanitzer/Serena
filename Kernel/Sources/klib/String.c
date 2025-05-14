//
//  String.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/1/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <kern/string.h>


ssize_t String_Length(const char* _Nonnull pStr)
{
    const char* p = pStr;

    while(*p++ != '\0');

    return p - pStr - 1;
}

ssize_t String_LengthUpTo(const char* _Nonnull pStr, ssize_t strsz)
{
    ssize_t len = 0;

    while (*pStr++ != '\0' && len < strsz) {
        len++;
    }

    return len;
}

// Copies the characters of 'pSrc' to 'pDst'. Returns a pointer that points to
// the first byte past the '\0' byte in the destination string. 
char* _Nonnull String_Copy(char* _Nonnull pDst, const char* _Nonnull pSrc)
{
    while (*pSrc != '\0') {
        *pDst++ = *pSrc++;
    }
    *pDst++ = '\0';

    return pDst;
}

char* _Nonnull String_CopyUpTo(char* _Nonnull pDst, const char* _Nonnull pSrc, ssize_t count)
{
    while (*pSrc != '\0' && count > 0) {
        *pDst++ = *pSrc++;
        count--;
    }
    if (count > 0) {
        *pDst++ = '\0';
    }

    return pDst;
}

bool String_Equals(const char* _Nonnull pLhs, const char* _Nonnull pRhs)
{
    while (*pLhs != '\0' && *pLhs == *pRhs) {
        pLhs++;
        pRhs++;
    }

    return (*pLhs == *pRhs) ? true : false;
}

bool String_EqualsUpTo(const char* _Nonnull pLhs, const char* _Nonnull pRhs, ssize_t count)
{
    while (count > 0 && *pLhs != '\0' && *pLhs == *pRhs) {
        pLhs++;
        pRhs++;
        count--;
    }

    return (count == 0 || *pLhs == *pRhs) ? true : false;
}

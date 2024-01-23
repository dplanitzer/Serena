//
//  xtoa.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <__stddef.h>
#include "printf.h"


const char* gLowerDigits = "0123456789abcdef";
const char* gUpperDigits = "0123456789ABCDEF";

const char *__lltoa(int64_t val, int radix, bool isUppercase, int fieldWidth, char paddingChar, char *pString, size_t maxLength)
{
    char *p0 = &pString[__max(maxLength - fieldWidth - 1, 0)];
    char *p = &pString[maxLength - 1];
    const char* digits = (isUppercase) ? gUpperDigits : gLowerDigits;
    int64_t absval = (val < 0) ? -val : val;
    int64_t q, r;
    
    if (val < 0) {
        p0--;
    }
    
    pString[maxLength - 1] = '\0';
    do {
        p--;
        __Int64_DivMod(absval, radix, &q, &r);
        *p = digits[r];
        absval = q;
    } while (absval && p >= p0);
    
    if (val < 0) {
        p--;
        *p = '-';
        // Convert a zero padding char to a space 'cause zero padding doesn't make
        // sense if the number is negative.
        if (paddingChar == '0') {
            paddingChar = ' ';
        }
    }
    
    if (paddingChar != '\0') {
        while (p > p0) {
            p--;
            *p = paddingChar;
        }
    }
    
    return __max(p, p0);
}

const char *__ulltoa(uint64_t val, int radix, bool isUppercase, int fieldWidth, char paddingChar, char *pString, size_t maxLength)
{
    char *p0 = &pString[__max(maxLength - fieldWidth - 1, 0)];
    char *p = &pString[maxLength - 1];
    const char* digits = (isUppercase) ? gUpperDigits : gLowerDigits;
    int64_t q, r;
    
    pString[maxLength - 1] = '\0';
    do {
        p--;
        __Int64_DivMod(val, radix, &q, &r);
        *p = digits[r];
        val = q;
    } while (val && p >= p0);
    
    if (paddingChar != '\0') {
        while (p > p0) {
            p--;
            *p = paddingChar;
        }
    }
    
    return __max(p, p0);
}

char *itoa(int val, char *buf, int radix)
{
    if (radix != 8 && radix != 10 && radix != 16 || buf == NULL) {
        return NULL;
    }

    char t[12];
    const char* p = __lltoa(val, radix, 0, 11, 0, t, sizeof(t));
    while (*p != '\0') { *buf++ = *p++; }
    *buf = '\0';
}

char *ltoa(long val, char *buf, int radix)
{
    if (radix != 8 && radix != 10 && radix != 16 || buf == NULL) {
        return NULL;
    }

    char t[12];
    const char* p = __lltoa(val, radix, 0, 11, 0, t, sizeof(t));

    while (*p != '\0') { *buf++ = *p++; }
    *buf = '\0';
}

char *lltoa(long long val, char *buf, int radix)
{
    if (radix != 8 && radix != 10 && radix != 16 || buf == NULL) {
        return NULL;
    }

    char t[22];
    const char* p = __lltoa(val, radix, 0, 21, 0, t, sizeof(t));
    while (*p != '\0') { *buf++ = *p++; }
    *buf = '\0';
}

//
//  xtoa.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <stdint.h>
#include "printf.h"

#define abs(x) (((x) < 0) ? -(x) : (x))
#define min(x, y) (((x) < (y) ? (x) : (y)))
#define max(x, y) (((x) > (y) ? (x) : (y)))

extern int Int64_DivMod(int64_t dividend, int64_t divisor, int64_t *quotient, int64_t *remainder);


const char* gLowerDigits = "0123456789abcdef";
const char* gUpperDigits = "0123456789ABCDEF";

const char *__lltoa(int64_t val, int base, bool isUppercase, int fieldWidth, char paddingChar, char *pString, size_t maxLength)
{
    char *p0 = &pString[max(maxLength - fieldWidth - 1, 0)];
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
        Int64_DivMod(absval, base, &q, &r);
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
    
    return max(p, p0);
}

const char *__ulltoa(uint64_t val, int base, bool isUppercase, int fieldWidth, char paddingChar, char *pString, size_t maxLength)
{
    char *p0 = &pString[max(maxLength - fieldWidth - 1, 0)];
    char *p = &pString[maxLength - 1];
    const char* digits = (isUppercase) ? gUpperDigits : gLowerDigits;
    int64_t q, r;
    
    pString[maxLength - 1] = '\0';
    do {
        p--;
        Int64_DivMod(val, base, &q, &r);
        *p = digits[r];
        val = q;
    } while (val && p >= p0);
    
    if (paddingChar != '\0') {
        while (p > p0) {
            p--;
            *p = paddingChar;
        }
    }
    
    return max(p, p0);
}

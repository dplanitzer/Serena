//
//  atox.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>


int atoi(const char *str)
{
    return (int) atol(str);
}

long atol(const char *str)
{
    // Skip whitespace
    while (*str != '\0' && isspace(*str)) {
        str++;
    }


    // Detect the sign
    bool isNegative = false;
    if (*str == '-') {
        str++;
        isNegative = true;
    } else if (*str == '+') {
        str++;
    }


    // Convert digits
    long val = 0;
    for (;;) {
        const char ch = *str++;

        if (ch == '\0' || ch < '0' || ch > '9') {
            break;
        }
        val *= 10;
        val += (ch - '0');
    }

    return (isNegative) ? -val : val;
}

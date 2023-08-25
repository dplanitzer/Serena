//
//  ctype.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <ctype.h>

#define IS_ALPHA    ((unsigned short) 1)
#define IS_LOWER    ((unsigned short) 2)
#define IS_UPPER    ((unsigned short) 4)
#define IS_DIGIT    ((unsigned short) 8)
#define IS_XDIGIT   ((unsigned short) 16)
#define IS_CNTRL    ((unsigned short) 32)
#define IS_SPACE    ((unsigned short) 64)
#define IS_BLANK    ((unsigned short) 128)
#define IS_PRINT    ((unsigned short) 256)
#define IS_PUNCT    ((unsigned short) 512)
#define IS_GRAPH    ((unsigned short) 1024)
#define IS_ALNUM    ((unsigned short) 2048)

// <https://en.cppreference.com/w/c/string/byte>
static const unsigned short gCharClasses[128] = {
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,                                                           // 0x08
    IS_CNTRL|IS_SPACE|IS_BLANK,
    IS_CNTRL|IS_SPACE,
    IS_CNTRL|IS_SPACE,
    IS_CNTRL|IS_SPACE,
    IS_CNTRL|IS_SPACE,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,                                                           // 0x10
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_CNTRL,
    IS_PRINT|IS_SPACE|IS_BLANK,                                         // 0x20
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_DIGIT|IS_XDIGIT,                      // 0x30
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_DIGIT|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_DIGIT|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_DIGIT|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_DIGIT|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_DIGIT|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_DIGIT|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_DIGIT|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_DIGIT|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_DIGIT|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,                                         // 0x40
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,                       // 0x50
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_UPPER,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,                                         // 0x60
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER|IS_XDIGIT,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,                       // 0x70
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_ALNUM|IS_ALPHA|IS_LOWER,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_PRINT|IS_GRAPH|IS_PUNCT,
    IS_CNTRL                                                            // 0x7f
};

int isalnum(int ch)
{
    if (ch >= 0 && ch < 128) {
        return ((gCharClasses[ch] & IS_ALNUM) == IS_ALNUM) ? -1 : 0;
    } else {
        return 0;
    }
}

int isalpha(int ch)
{
    if (ch >= 0 && ch < 128) {
        return ((gCharClasses[ch] & IS_ALPHA) == IS_ALPHA) ? -1 : 0;
    } else {
        return 0;
    }
}

int islower(int ch)
{
    if (ch >= 0 && ch < 128) {
        return ((gCharClasses[ch] & IS_LOWER) == IS_LOWER) ? -1 : 0;
    } else {
        return 0;
    }
}

int isupper(int ch)
{
    if (ch >= 0 && ch < 128) {
        return ((gCharClasses[ch] & IS_UPPER) == IS_UPPER) ? -1 : 0;
    } else {
        return 0;
    }
}

int isdigit(int ch)
{
    if (ch >= 0 && ch < 128) {
        return ((gCharClasses[ch] & IS_DIGIT) == IS_DIGIT) ? -1 : 0;
    } else {
        return 0;
    }
}

int isxdigit(int ch)
{
    if (ch >= 0 && ch < 128) {
        return ((gCharClasses[ch] & IS_XDIGIT) == IS_XDIGIT) ? -1 : 0;
    } else {
        return 0;
    }
}

int iscntrl(int ch)
{
    if (ch >= 0 && ch < 128) {
        return ((gCharClasses[ch] & IS_CNTRL) == IS_CNTRL) ? -1 : 0;
    } else {
        return 0;
    }
}

int isgraph(int ch)
{
    if (ch >= 0 && ch < 128) {
        return ((gCharClasses[ch] & IS_GRAPH) == IS_GRAPH) ? -1 : 0;
    } else {
        return 0;
    }
}

int isspace(int ch)
{
    if (ch >= 0 && ch < 128) {
        return ((gCharClasses[ch] & IS_SPACE) == IS_SPACE) ? -1 : 0;
    } else {
        return 0;
    }
}

int isblank(int ch)
{
    if (ch >= 0 && ch < 128) {
        return ((gCharClasses[ch] & IS_BLANK) == IS_BLANK) ? -1 : 0;
    } else {
        return 0;
    }
}

int isprint(int ch)
{
    if (ch >= 0 && ch < 128) {
        return ((gCharClasses[ch] & IS_PRINT) == IS_PRINT) ? -1 : 0;
    } else {
        return 0;
    }
}

int ispunct(int ch)
{
    if (ch >= 0 && ch < 128) {
        return ((gCharClasses[ch] & IS_PUNCT) == IS_PUNCT) ? -1 : 0;
    } else {
        return 0;
    }
}

int tolower(int ch)
{
    if (ch >= 0 && ch < 128 && ((gCharClasses[ch] & IS_UPPER) == IS_UPPER)) {
        return ch + 32;
    }
    return ch;
}

int toupper(int ch)
{
    if (ch >= 0 && ch < 128 && ((gCharClasses[ch] & IS_LOWER) == IS_LOWER)) {
        return ch - 32;
    }
    return ch;
}

//
//  ctype.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#define _CTYPE_NO_MACROS
#include <ctype.h>

// <https://en.cppreference.com/w/c/string/byte>
const unsigned short __C_LOCALE_CC[128] = {
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,                                              // 0x08
    __IS_CNTRL|__IS_SPACE|__IS_BLANK,
    __IS_CNTRL|__IS_SPACE,
    __IS_CNTRL|__IS_SPACE,
    __IS_CNTRL|__IS_SPACE,
    __IS_CNTRL|__IS_SPACE,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,                                              // 0x10
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_CNTRL,
    __IS_PRINT|__IS_SPACE|__IS_BLANK,                        // 0x20
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_DIGIT|__IS_XDIGIT,            // 0x30
    __IS_PRINT|__IS_GRAPH|__IS_DIGIT|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_DIGIT|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_DIGIT|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_DIGIT|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_DIGIT|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_DIGIT|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_DIGIT|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_DIGIT|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_DIGIT|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,                        // 0x40
    __IS_PRINT|__IS_GRAPH|__IS_UPPER|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,                       // 0x50
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_UPPER,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,                        // 0x60
    __IS_PRINT|__IS_GRAPH|__IS_LOWER|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER|__IS_XDIGIT,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,                       // 0x70
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_LOWER,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_PRINT|__IS_GRAPH|__IS_PUNCT,
    __IS_CNTRL                                               // 0x7f
};

int isalnum(int ch)
{
    return _isalnum(ch);
}

int isalpha(int ch)
{
    return _isalpha(ch);
}

int islower(int ch)
{
    return _islower(ch);
}

int isupper(int ch)
{
    return _isupper(ch);
}

int isdigit(int ch)
{
    return _isdigit(ch);
}

int isxdigit(int ch)
{
    return _isxdigit(ch);
}

int iscntrl(int ch)
{
    return _iscntrl(ch);
}

int isgraph(int ch)
{
    return _isgraph(ch);
}

int isspace(int ch)
{
    return _isspace(ch);
}

int isblank(int ch)
{
    return _isblank(ch);
}

int isprint(int ch)
{
    return _isprint(ch);
}

int ispunct(int ch)
{
    return _ispunct(ch);
}

int tolower(int ch)
{
    return _tolower(ch);
}

int toupper(int ch)
{
    return _toupper(ch);
}

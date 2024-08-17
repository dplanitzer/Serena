//
//  ctype.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _CTYPE_H
#define _CTYPE_H 1

#include <System/_cmndef.h>

__CPP_BEGIN

extern int isalnum(int ch);
extern int isalpha(int ch);
extern int islower(int ch);
extern int isupper(int ch);
extern int isdigit(int ch);
extern int isxdigit(int ch);
extern int iscntrl(int ch);
extern int isgraph(int ch);
extern int isspace(int ch);
extern int isblank(int ch);
extern int isprint(int ch);
extern int ispunct(int ch);

extern int tolower(int ch);
extern int toupper(int ch);


extern const unsigned short __gCharClasses[128];

#define __IS_ALPHA  ((unsigned short) 1)
#define __IS_LOWER  ((unsigned short) 2)
#define __IS_UPPER  ((unsigned short) 4)
#define __IS_DIGIT  ((unsigned short) 8)
#define __IS_XDIGIT ((unsigned short) 16)
#define __IS_CNTRL  ((unsigned short) 32)
#define __IS_SPACE  ((unsigned short) 64)
#define __IS_BLANK  ((unsigned short) 128)
#define __IS_PRINT  ((unsigned short) 256)
#define __IS_PUNCT  ((unsigned short) 512)
#define __IS_GRAPH  ((unsigned short) 1024)
#define __IS_ALNUM  ((unsigned short) 2048)


#define _isalnum(ch) \
((((ch) & 0xffffff80) == 0) ? ((__gCharClasses[ch] & __IS_ALNUM) == __IS_ALNUM) ? -1 : 0 : 0)

#define _isalpha(ch) \
((((ch) & 0xffffff80) == 0) ? ((__gCharClasses[ch] & __IS_ALPHA) == __IS_ALPHA) ? -1 : 0 : 0)

#define _islower(ch) \
((((ch) & 0xffffff80) == 0) ? ((__gCharClasses[ch] & __IS_LOWER) == __IS_LOWER) ? -1 : 0 : 0)

#define _isupper(ch) \
((((ch) & 0xffffff80) == 0) ? ((__gCharClasses[ch] & __IS_UPPER) == __IS_UPPER) ? -1 : 0 : 0)

#define _isdigit(ch) \
((((ch) & 0xffffff80) == 0) ? ((__gCharClasses[ch] & __IS_DIGIT) == __IS_DIGIT) ? -1 : 0 : 0)

#define _isxdigit(ch) \
((((ch) & 0xffffff80) == 0) ? ((__gCharClasses[ch] & __IS_XDIGIT) == __IS_XDIGIT) ? -1 : 0 : 0)

#define _iscntrl(ch) \
((((ch) & 0xffffff80) == 0) ? ((__gCharClasses[ch] & __IS_CNTRL) == __IS_CNTRL) ? -1 : 0 : 0)

#define _isgraph(ch) \
((((ch) & 0xffffff80) == 0) ? ((__gCharClasses[ch] & __IS_GRAPH) == __IS_GRAPH) ? -1 : 0 : 0)

#define _isspace(ch) \
((((ch) & 0xffffff80) == 0) ? ((__gCharClasses[ch] & __IS_SPACE) == __IS_SPACE) ? -1 : 0 : 0)

#define _isblank(ch) \
((((ch) & 0xffffff80) == 0) ? ((__gCharClasses[ch] & __IS_BLANK) == __IS_BLANK) ? -1 : 0 : 0)

#define _isprint(ch) \
((((ch) & 0xffffff80) == 0) ? ((__gCharClasses[ch] & __IS_PRINT) == __IS_PRINT) ? -1 : 0 : 0)

#define _ispunct(ch) \
((((ch) & 0xffffff80) == 0) ? ((__gCharClasses[ch] & __IS_PUNCT) == __IS_PUNCT) ? -1 : 0 : 0)


#define _tolower(ch) \
((((ch) & 0xffffff80) == 0 && ((__gCharClasses[ch] & __IS_UPPER) == __IS_UPPER)) ? (ch) + 32 : ch)

#define _toupper(ch) \
((((ch) & 0xffffff80) == 0 && ((__gCharClasses[ch] & __IS_LOWER) == __IS_LOWER)) ? (ch) - 32 : ch)

__CPP_END

#endif /* _CTYPE_H */


// The following definitions depend on a switch that the includer sets before
// including this file.

__CPP_BEGIN

#ifndef _CTYPE_NO_MACROS
#define isalnum(ch) _isalnum(ch)
#define isalpha(ch) _isalpha(ch)
#define islower(ch) _islower(ch)
#define isupper(ch) _isupper(ch)
#define isdigit(ch) _isdigit(ch)
#define isxdigit(ch) _isxdigit(ch)
#define iscntrl(ch) _iscntrl(ch)
#define isgraph(ch) _isgraph(ch)
#define isspace(ch) _isspace(ch)
#define isblank(ch) _isblank(ch)
#define isprint(ch) _isprint(ch)
#define ispunct(ch) _ispunct(ch)

#define tolower(ch) _tolower(ch)
#define toupper(ch) _toupper(ch)
#endif /* _CTYPE_NO_MACROS */

__CPP_END

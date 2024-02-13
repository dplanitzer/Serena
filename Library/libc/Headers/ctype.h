//
//  ctype.h
//  Apollo
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

__CPP_END

#endif /* _CTYPE_H */

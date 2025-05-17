//
//  utils.h
//  snake
//
//  Created by Dietmar Planitzer on 3/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <stdbool.h>
#include <_cmndef.h>

extern char *__strcpy(char * _Restrict dst, const char * _Restrict src);
extern char *__strcat(char * _Restrict dst, const char * _Restrict src);


extern void cursor_on(bool onOff);
extern char* cls(char* dst);
extern char* mv_by_precomp(char* dst, const char* leb);
extern char* mv_to(char* dst, int x, int y);
extern char* h_line(char* dst, char ch, int count);

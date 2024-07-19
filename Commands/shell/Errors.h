//
//  Errors.h
//  sh
//
//  Created by Dietmar Planitzer on 7/7/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Errors_h
#define Errors_h

#include <System/Error.h>
#include <errno.h>

#define ESYNTAX     -1
#define EUNDERFLOW  -2
#define EUNDEFINED  -3
#define EREDEFINED  -4
#define EUNCMD      -5

#define _EFIRST_SHELL   ESYNTAX
#define _ELAST_SHELL    EUNCMD

extern const char *shell_strerror(int err_no);

#endif  /* Errors_h */

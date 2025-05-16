//
//  Errors.h
//  sh
//
//  Created by Dietmar Planitzer on 7/7/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Errors_h
#define Errors_h

#include <sys/errno.h>


#define ESYNTAX         -1
#define EUNDERFLOW      -2
#define EUNDEFVAR       -3
#define EREDEFVAR       -4
#define ENOCMD          -5
#define ENOTIMPL        -6
#define ENOSCOPE        -7
#define ETYPEMISMATCH   -8
#define EDIVBYZERO      -9
#define ENOTLVALUE      -10
#define ENOASSIGN       -11
#define EIMMUTABLE      -12
#define ENOVAL          -13
#define EBREAK          -14
#define ECONTINUE       -15
#define ENOTLOOP        -16

#define _EFIRST_SHELL   ESYNTAX
#define _ELAST_SHELL    ENOTLOOP

extern const char *shell_strerror(int err_no);

#endif  /* Errors_h */

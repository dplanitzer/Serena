//
//  Errors.c
//  sh
//
//  Created by Dietmar Planitzer on 7/7/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Errors.h"
#include <string.h>


const char *shell_strerror(int err_no)
{
    static const char* gErrorDescs[] = {
        /*EREDEFINED*/      "Variable redefinition",
        /*EUNDEFINED*/      "Undefined variable",
        /*EUNDERFLOW*/      "Stack underflow",
        /*ESYNTAX*/         "Syntax error",
    };

    if (err_no >= _ELAST_SHELL && err_no <= _EFIRST_SHELL) {
        return gErrorDescs[err_no - _ELAST_SHELL];
    } else {
        return (const char*)strerror(err_no);
    }
}
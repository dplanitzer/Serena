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
        /*ESYNTAX*/         "Syntax error",
        /*EUNDERFLOW*/      "Underflow",
        /*EUNDEFINED*/      "Undefined",
        /*EREDEFINED*/      "Redefinition",
    };

    if (err_no >= _EFIRST_SHELL && err_no <= _ELAST_SHELL) {
        return gErrorDescs[err_no + _EFIRST_SHELL];
    } else {
        return (const char*)strerror(err_no);
    }
}
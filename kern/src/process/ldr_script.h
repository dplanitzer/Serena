//
//  ldr_script.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/21/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _LDR_SCRIPT_H
#define _LDR_SCRIPT_H

#include "proc_img.h"

// Checks whether an executable file is actually a shell script with a shebang
// '#!' line. If so it replaces the current argv[0] with the interpreter path,
// optional interpreter argument and the script file path. It then tells the
// caller that it should continue to find an actual loader to load the
// interpreter with the adjusted argument vector.
extern errno_t ldr_script_load(proc_img_t* _Nonnull pimg);

#endif /* _LDR_SCRIPT_H */

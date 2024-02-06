//
//  _syslimits.h
//  libsystem
//
//  Created by Dietmar Planitzer on 9/15/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __ABI_SYSLIMITS_H
#define __ABI_SYSLIMITS_H 1

// Maximum size of the command line arguments and environment that can be passed to a process
#define __ARG_MAX               65535

// Max length of a path including the terminating NUL character
#define __PATH_MAX              4096

// Max length of a path component (file or directory name) without the terminating NUL character
#define __PATH_COMPONENT_MAX    256

#endif /* __ABI_SYSLIMITS_H */

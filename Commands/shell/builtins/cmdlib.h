//
//  cmdlib.h
//  sh
//
//  Created by Dietmar Planitzer on 5/20/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef cmdlib_h
#define cmdlib_h

#include <System/Error.h>
#include <stdlib.h>

#define exit_code(__err) \
((__err) == EOK) ? EXIT_SUCCESS : EXIT_FAILURE

extern void print_error(const char* _Nonnull cmd, const char* _Nullable path, errno_t err);

#endif  /* cmdlib_h */

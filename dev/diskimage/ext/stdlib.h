//
//  ext/stdlib.h
//  diskimage
//
//  Created by Dietmar Planitzer on 1/29/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#if defined(_WIN32) || defined(_WIN64)
#include <stdlib.h> // itoa(), ltoa()
#endif

#if defined(__APPLE__)
#include <../../user/lib/libc/h/ext/stdlib.h>
#endif

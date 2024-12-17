//
//  klib_Error.h
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef klib_Error_h
#define klib_Error_h

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <System/Error.h>

#define _Try_bang(f)         { const errno_t _err_ = (f);  if (_err_ != 0) { printf("%s:%d:%s\n", __FILE__, __LINE__, __func__); abort(); }}

#ifndef __cplusplus
#define try_bang(f)  _Try_bang(f)
#endif  /* __cplusplus */

#endif /* klib_Error_h */

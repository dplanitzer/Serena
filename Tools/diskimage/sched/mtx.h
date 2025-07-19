//
//  mtx.h
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef di_mtx_h
#define di_mtx_h

#include <windows.h>

typedef SRWLOCK mtx_t; 

extern void mtx_init(mtx_t* self);
extern void mtx_deinit(mtx_t* self);
extern void mtx_lock(mtx_t* self);
extern void mtx_unlock(mtx_t* self);

#endif /* di_mtx_h */

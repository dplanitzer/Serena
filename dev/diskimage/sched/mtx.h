//
//  mtx.h
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef di_mtx_h
#define di_mtx_h

#ifdef _WIN32
#include <windows.h>

typedef SRWLOCK mtx_t; 
#else
#include <pthread.h>

typedef pthread_mutex_t mtx_t;
#endif

extern void mtx_init(mtx_t* self);
extern void mtx_deinit(mtx_t* self);
extern void mtx_lock(mtx_t* self);
extern void mtx_unlock(mtx_t* self);

#endif /* di_mtx_h */

//
//  diskimage.h
//  diskimage
//
//  Created by Dietmar Planitzer on 3/14/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef diskimage_h
#define diskimage_h

#include <stdbool.h>
#include <stdint.h>
#include <System/Error.h>

typedef struct di_direntry {
    const char* name;
    uint64_t    fileSize;
} di_direntry;

typedef errno_t (*di_begin_directory_callback)(void* _Nullable ctx, const di_direntry* _Nonnull entry, void* pToken, void** pOutToken);
typedef errno_t (*di_end_directory_callback)(void* _Nullable ctx, void* pToken);
typedef errno_t (*di_file_callback)(void* _Nullable ctx, const di_direntry* _Nonnull entry, void* pToken);

typedef struct di_iterate_directory_callbacks {
    void * _Nullable context;
    
    di_begin_directory_callback beginDirectory;
    di_end_directory_callback   endDirectory;
    di_file_callback            file;
} di_iterate_directory_callbacks;

extern errno_t di_iterate_directory(const char* _Nonnull rootPath, const di_iterate_directory_callbacks* _Nonnull cb, void* _Nullable pInitialToken);

#endif /* diskimage_h */

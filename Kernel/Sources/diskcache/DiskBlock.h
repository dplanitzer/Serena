//
//  DiskBlock.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/30/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DiskBlock_h
#define DiskBlock_h

#include <kobj/Object.h>
#include <klib/List.h>
#include <filesystem/FSBlock.h>


typedef enum DiskBlockOp {
    kDiskBlockOp_Idle = 0,
    kDiskBlockOp_Read = 1,
    kDiskBlockOp_Write = 2
} DiskBlockOp;


// General locking model:
// Management state is protected by the interlock
// Block data and error status is protected by the shared/exclusive lock
typedef struct DiskBlock {
    ListNode                        hashNode;           // Protected by Interlock
    ListNode                        lruNode;            // Protected by Interlock
    DiskDriverRef _Nonnull _Weak    disk;               // Protected by Interlock. Address by which a block is identified in the cache
    MediaId                         mediaId;            // Protected by Interlock. Address by which a block is identified in the cache
    LogicalBlockAddress             lba;                // Protected by Interlock. Address by which a block is identified in the cache
    int                             shareCount;         // Protected by Interlock
    struct __DiskBlockFlags {
        unsigned int        exclusive:1;    // Protected by Interlock
        unsigned int        hasData:1;      // Protected by Interlock
        unsigned int        isDirty:1;      // Protected by Interlock
        unsigned int        op:2;           // Protected by Interlock
        unsigned int        async:1;        // Protected by Interlock
        unsigned int        readError:8;    // Read: shared lock; Modify: exclusive lock
        unsigned int        reserved:18;
    }                               flags;
    uint8_t                         data[1];            // Read: shared lock; Modify: exclusive lock
} DiskBlock;


extern errno_t DiskBlock_Create(DiskDriverRef _Nonnull _Weak disk, MediaId mediaId, LogicalBlockAddress lba, size_t blockSize, DiskBlockRef _Nullable * _Nonnull pOutSelf);
extern void DiskBlock_Destroy(DiskBlockRef _Nullable self);

#define DiskBlock_InUse(__self) \
    ((__self)->shareCount > 0 || (__self)->flags.exclusive)

#define DiskBlock_Hash(__self) \
    DiskBlock_HashKey((__self)->disk, (__self)->mediaId, (__self)->lba)

#define DiskBlock_IsEqual(__self, __other) \
    DiskBlock_IsEqualKey(__self, (__other)->disk, (__other)->mediaId, (__other)->lba)

#define DiskBlock_Purge(__self) \
    (__self)->flags.hasData = 0

#define DiskBlock_SetDiskAddress(__self, __disk, __mediaId, __lba)\
    (__self)->disk = __disk;\
    (__self)->mediaId = __mediaId;\
    (__self)->lba = __lba;\
    DiskBlock_Purge(__self)


#define DiskBlock_HashKey(__disk, __mediaId, __lba) \
    (size_t)((intptr_t)(__disk) + (__mediaId) + (__lba))

#define DiskBlock_IsEqualKey(__self, __disk, __mediaId, __lba) \
    (((__self)->disk == (__disk) && (__self)->mediaId == (__mediaId) && (__self)->lba == (__lba)) ? true : false)

#endif /* DiskBlock_h */

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


typedef enum AcquireBlock {
    kAcquireBlock_ReadOnly,     // Acquire the disk block for reading only with no write back
    kAcquireBlock_Update,       // Acquire the disk block for a partial update and write back
    kAcquireBlock_Replace,      // Acquire the disk block for a full update where every byte will be replaced and written back
    kAcquireBlock_Cleared       // Acquire the disk block with every byte cleared, potential additional full or partial updates and write back
} AcquireBlock;


typedef enum WriteBlock {
    kWriteBlock_Sync,           // Write the disk block back to disk and wait for the write to finish
    kWriteBlock_Deferred        // Mark the disk block as needing write back but wait with the write back until a flush event happens or the block is needed for another disk address
} WriteBlock;


typedef enum DiskBlockOp {
    kDiskBlockOp_Idle = 0,
    kDiskBlockOp_Read = 1,
    kDiskBlockOp_Write = 2
} DiskBlockOp;


typedef struct DiskAddress {
    DiskId              diskId;
    MediaId             mediaId;
    LogicalBlockAddress lba;
} DiskAddress;


// General locking model:
// Management state is protected by the interlock
// Block data and error status is protected by the shared/exclusive lock
typedef struct DiskBlock {
    ListNode            hashNode;           // Protected by Interlock
    ListNode            lruNode;            // Protected by Interlock
    DiskAddress         virtualAddress;     // Protected by Interlock. Address by which a block is identified in the cache
    DiskAddress         physicalAddress;    // Protected by protocol: no mutation in idle state. Mutable in R/W state. DiskCache may mutate at the start of a R/W cycle; after that a driver may mutate it while being the block owner. Block must be locked shared or exclusive
    int                 useCount;           // Protected by Interlock
    int                 shareCount;         // Protected by Interlock
    struct __Flags {
        unsigned int        byteSize:16;    // Constant value
        unsigned int        exclusive:1;    // Protected by Interlock
        unsigned int        hasData:1;      // Protected by Interlock
        unsigned int        isDirty:1;      // Protected by Interlock
        unsigned int        op:2;           // Protected by Interlock
        unsigned int        async:1;        // Protected by Interlock
        unsigned int        readError:8;    // Read: shared lock; Modify: exclusive lock
        unsigned int        reserved:2;
    }                   flags;
    uint8_t             data[1];            // Read: shared lock; Modify: exclusive lock
} DiskBlock;


#define DiskBlock_GetData(__self) \
    (const void*)(&(__self)->data[0])

#define DiskBlock_GetMutableData(__self) \
    (void*)(&(__self)->data[0])

#define DiskBlock_GetByteSize(__self) \
    (__self)->flags.byteSize

#define DiskBlock_GetVirtualAddress(__self) \
    (&(__self)->virtualAddress)

#define DiskBlock_GetPhysicalAddress(__self) \
    (&(__self)->physicalAddress)

#define DiskBlock_GetOp(__self) \
    (DiskBlockOp)(__self)->flags.op


//
// Kernel internal functions
//

extern errno_t DiskBlock_Create(DiskId diskId, MediaId mediaId, LogicalBlockAddress lba, DiskBlockRef _Nullable * _Nonnull pOutSelf);
extern void DiskBlock_Destroy(DiskBlockRef _Nullable self);

#define DiskBlock_BeginUse(__self) \
    (__self)->useCount++

#define DiskBlock_EndUse(__self) \
    (__self)->useCount--

#define DiskBlock_InUse(__self) \
    ((__self)->useCount > 0)

#define DiskBlock_Hash(__self) \
    DiskBlock_HashKey((__self)->virtualAddress.diskId, (__self)->virtualAddress.mediaId, (__self)->virtualAddress.lba)

#define DiskBlock_IsEqual(__self, __other) \
    DiskBlock_IsEqualKey(__self, (__other)->virtualAddress.diskId, (__other)->virtualAddress.mediaId, (__other)->virtualAddress.lba)

#define DiskBlock_Purge(__self) \
    (__self)->flags.hasData = 0

#define DiskBlock_SetVirtualAddress(__self, __diskId, __mediaId, __lba)\
    (__self)->virtualAddress.diskId = diskId;\
    (__self)->virtualAddress.mediaId = __mediaId;\
    (__self)->virtualAddress.lba = __lba;\
    DiskBlock_Purge(__self)


#define DiskBlock_HashKey(__diskId, __mediaId, __lba) \
    (size_t)((__diskId) + (__mediaId) + (__lba))

#define DiskBlock_IsEqualKey(__self, __diskId, __mediaId, __lba) \
    (((__self)->virtualAddress.diskId == (__diskId) && (__self)->virtualAddress.mediaId == (__mediaId) && (__self)->virtualAddress.lba == (__lba)) ? true : false)

#endif /* DiskBlock_h */

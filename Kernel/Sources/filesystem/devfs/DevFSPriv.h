//
//  DevFSPriv.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DevFSPriv_h
#define DevFSPriv_h

#include "DevFS.h"
#include <dispatcher/Lock.h>
#include <dispatcher/SELock.h>
#include <filesystem/FSUtilities.h>
#include <klib/Types.h>
#include <klib/List.h>

#define MAX_LINK_COUNT      65535
#define MAX_NAME_LENGTH     10

typedef struct DfsItem {
    ListNode            inidChain;      // Inid hash chain link 
    TimeInterval        accessTime;
    TimeInterval        modificationTime;
    TimeInterval        statusChangeTime;
    FileOffset          size;
    InodeId             inid;
    int                 linkCount;
    FileType            type;
    uint8_t             flags;
    FilePermissions     permissions;
    UserId              uid;
    GroupId             gid;
} DfsItem;

// A directory entry
typedef struct DfsDirectoryEntry {
    ListNode    sibling;
    InodeId     inid;
    int8_t      nameLength;
    char        name[MAX_NAME_LENGTH];
} DfsDirectoryEntry;

// A directory of drivers and child directories
typedef struct DfsDirectoryItem {
    DfsItem super;
    List    entries;
} DfsDirectoryItem;

// A driver entry
typedef struct DfsDriverItem {
    DfsItem             super;
    DriverRef _Nonnull  instance;
    intptr_t            arg;
} DfsDriverItem;


//
// Inode Extensions
//

// Returns the DfsItem backing the inode.
#define Inode_GetDfsItem(__self) \
    Inode_GetRefConAs(__self, DfsItem*)

#define Inode_GetDfsDirectoryItem(__self) \
    Inode_GetRefConAs(__self, DfsDirectoryItem*)

#define Inode_GetDfsDriverItem(__self) \
    Inode_GetRefConAs(__self, DfsDriverItem*)


//
// DevFS
//

#define INID_HASH_CHAINS_COUNT  8
#define INID_HASH_CODE(__id)    (__id)
#define INID_HASH_INDEX(__id)   (INID_HASH_CODE(__id) & (INID_HASH_CHAINS_COUNT-1))


// DevFS Locking:
//
// seLock:  provides exclusion for mount, unmount and acquire-root-node
final_class_ivars(DevFS, Filesystem,
    SELock      seLock;
    List        inidChains[INID_HASH_CHAINS_COUNT];
    InodeId     rootDirInodeId;
    InodeId     nextAvailableInodeId;
    struct {
        unsigned int    isMounted:1;
        unsigned int    reserved: 31;
    }           flags;
);


InodeId DevFS_GetNextAvailableInodeId(DevFSRef _Nonnull _Locked self);

void DevFS_AddItem(DevFSRef _Nonnull _Locked self, DfsItem* _Nonnull item);
void DevFS_RemoveItem(DevFSRef _Nonnull _Locked self, InodeId inid);
DfsItem* _Nullable DevFS_GetItem(DevFSRef _Nonnull _Locked self, InodeId inid);

extern errno_t DevFS_createNode(DevFSRef _Nonnull self, FileType type, User user, FilePermissions permissions, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, void* _Nullable dirInsertionHint, InodeRef _Nullable * _Nonnull pOutNode);
extern errno_t DevFS_onReadNodeFromDisk(DevFSRef _Nonnull self, InodeId id, InodeRef _Nullable * _Nonnull pOutNode);
extern errno_t DevFS_onWriteNodeToDisk(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode);
extern void DevFS_onRemoveNodeFromDisk(DevFSRef _Nonnull self, InodeRef _Nonnull pNode);

extern errno_t DevFS_InsertDirectoryEntry(DevFSRef _Nonnull _Locked self, InodeRef _Nonnull _Locked pDir, InodeId inid, const PathComponent* _Nonnull pName);
extern errno_t DevFS_RemoveDirectoryEntry(DevFSRef _Nonnull _Locked self, InodeRef _Nonnull _Locked pDir, InodeId idToRemove);

extern errno_t DevFS_acquireRootDirectory(DevFSRef _Nonnull self, InodeRef _Nullable * _Nonnull pOutDir);
extern errno_t DevFS_acquireNodeForName(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, User user, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode);
extern errno_t DevFS_getNameOfNode(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, InodeId id, User user, MutablePathComponent* _Nonnull pName);
extern errno_t DevFS_openDirectory(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, User user);
extern errno_t DevFS_readDirectory(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, void* _Nonnull pBuffer, ssize_t nBytesToRead, FileOffset* _Nonnull pInOutOffset, ssize_t* _Nonnull nOutBytesRead);

extern void DfsItem_Destroy(DfsItem* _Nullable self);

extern errno_t DfsDirectoryItem_Create(InodeId inid, FilePermissions permissions, UserId uid, GroupId gid, InodeId pnid, DfsDirectoryItem* _Nullable * _Nonnull pOutSelf);
extern void DfsDirectoryItem_Destroy(DfsDirectoryItem* _Nullable self);
extern bool DfsDirectoryItem_IsEmpty(DfsDirectoryItem* _Nonnull self);
extern errno_t _Nullable DfsDirectoryItem_GetEntryForName(DfsDirectoryItem* _Nonnull self, const PathComponent* _Nonnull pc, DfsDirectoryEntry* _Nullable * _Nonnull pOutEntry);
extern errno_t DfsDirectoryItem_GetNameOfEntryWithId(DfsDirectoryItem* _Nonnull self, InodeId inid, MutablePathComponent* _Nonnull mpc);
extern errno_t DfsDirectoryItem_AddEntry(DfsDirectoryItem* _Nonnull self, InodeId inid, const PathComponent* _Nonnull pc);
extern errno_t DfsDirectoryItem_RemoveEntry(DfsDirectoryItem* _Nonnull self, InodeId inid);

extern errno_t DfsDriverItem_Create(InodeId inid, FilePermissions permissions, UserId uid, GroupId gid, DriverRef _Nonnull pDriver, intptr_t arg, DfsDriverItem* _Nullable * _Nonnull pOutSelf);
extern void DfsDriverItem_Destroy(DfsDriverItem* _Nullable self);

#endif /* DevFSPriv_h */

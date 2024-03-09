//
//  SerenaFSPriv.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/7/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef SerenaFSPriv_h
#define SerenaFSPriv_h

#include "SerenaFS.h"
#include <dispatcher/ConditionVariable.h>
#include <dispatcher/Lock.h>

#define kSFSMaxFilenameLength               28
#define kSFSBlockSizeShift                  9
#define kSFSBlockSize                       (1 << kSFSBlockSizeShift)
#define kSFSBlockSizeMask                   (kSFSBlockSize - 1)
#define kSFSDirectoryEntriesPerBlock        (kSFSBlockSize / sizeof(SFSDirectoryEntry))
#define kSFSDirectoryEntriesPerBlockMask    (kSFSDirectoryEntriesPerBlock - 1)
#define kSFSMaxDirectDataBlockPointers      114


//
// Serena FS On-Disk Format
//


// Meaning of 'reserved' bytes:
// * Ignore on read
// * Set to 0 when formatting
// * Do not modify on write (preserve whatever values the reserved bytes have)

enum {
    kSFSVolume_MinBlockCount = 4,   // Need space for at least 1 volume header block + 1 allocation bitmap block + 1 root dir inode + 1 root dir content block
};


//
// Volume Header
//
// The volume header is stored in logical block #0 on the disk. It stores data
// to identify the filesystem type and version and to locate the root directory
// and other files needed to manage the filesystem.

enum {
    kSFSSignature_SerenaFS = 0x53654653,        // 'SeFS'
};

// Semantic FS version. Encoded in a 32bit integer as:
// xx_MA_MI_PA
// where MA is the major version, MI the minor and PA the patch version. Each
// version field occupies exactly one byte and each sub-version field is treated
// as a unsigned binary encoded number.
enum {
    kSFSVersion_v1 = 0x00010000,                // v1.0.0
    kSFSVersion_Current = kSFSVersion_v1,       // Version to use for formatting a new disk
};

enum {
    kSFSVolumeAttributeBit_ReadOnly = 0,        // If set then the volume is (software) write protected. A volume is R/W-able if it is neither software nor hardware read-only
    kSFSVolumeAttributeBit_IsConsistent = 1,    // OnMount() must clear this bit on the disk and onUnmount must set it on disk as the last write operation. If this bit is cleared on mount then the FS state on disk should be considered inconsistent
};

typedef struct SFSVolumeHeader {
    uint32_t            signature;
    uint32_t            version;
    uint32_t            attributes;

    TimeInterval        creationTime;               // Date/time when the disk was formatted to create the FS
    TimeInterval        modificationTime;           // Date/time when the most recent modification to the FS happened
    
    uint32_t            blockSize;                  // Allocation block size (currently always == disk block size)
    LogicalBlockCount   volumeBlockCount;           // Size of the volume in terms of allocation blocks
    uint32_t            allocationBitmapByteSize;   // Size of allocation bitmap in bytes (XXX tmp until we'll turn the allocation bitmap into a real file)

    LogicalBlockAddress rootDirectory;              // LBA of the root directory Inode
    LogicalBlockAddress allocationBitmap;           // LBA of the first block of the allocation bitmap area
    // All bytes from here to the end of the block are reserved
} SFSVolumeHeader;


//
// Allocation Bitmap
//
// The allocation bitmap is stored in a sequential set of blocks. There is no
// inode for an allocation bitmap. Each bit in a block corresponds to a block
// on the disk. The LBA of the on-disk block is used to address its corresponding
// bit like this:
//    byteOffset = lba / 8
//    bitInByte  = 7 - (lba % 8)
//    blockNo    = byteOffset / kBlockSize
// The number of blocks needed for the allocation bitmap is calculated like
// this:
//    blockCount = ((lbaCount + 7) / 8 + (BlockSize - 1)) / BlockSize
// 0 means that the block is available and 1 means that it is allocated.
// All blocks on the disk including the volume header block and the allocation
// bitmap itself are covered by the allocation bitmap.


//
// Inodes
//
// NOTE: disk nodes own the data blocks of a file/directory. Inodes are set up
// with a pointer to the disk node block map. So inodes manipulate the block map
// directly instead of copying it back and forth. That's okay because the inode
// lock effectively protects the disk node sitting behind the inode. 

typedef struct SFSBlockMap {
    LogicalBlockAddress p[kSFSMaxDirectDataBlockPointers];
} SFSBlockMap;

typedef struct SFSInode {
    TimeInterval        accessTime;
    TimeInterval        modificationTime;
    TimeInterval        statusChangeTime;
    FileOffset          size;
    UserId              uid;
    GroupId             gid;
    FilePermissions     permissions;
    int                 linkCount;
    FileType            type;
    SFSBlockMap         blockMap;
} SFSInode;
typedef SFSInode* SFSInodeRef;


//
// Files
//
// A file consists of metadata and file content. The metadata is represented by
// a SFSInode which is stored in a separate block. The file contents is stored
// in a independent set of blocks.
//
// The inode id of a file is the LBA of the bloc that holds the inode data.


//
// Directory File
//
// A directory file stores an array of SFSDirectoryEntry objects in its file
// content.
// Internal organisation:
// [0] "."
// [1] ".."
// [2] userEntry0
// .
// [n] userEntryN-1
// This should be mod(SFSDiskBlockSize, SFSDirectoryEntrySize) == 0
// The number of entries in the directory file is fileLength / sizeof(SFSDirectoryEntry)
//
// The '.' and '..' entries of the root directory map to the root directory
// inode id.
typedef struct SFSDirectoryEntry {
    LogicalBlockAddress id;
    char                filename[kSFSMaxFilenameLength];    // if strlen(filename) < kSFSMaxFilenameLength -> 0 terminated
} SFSDirectoryEntry;



//
// Directories
//

enum SFSDirectoryQueryKind {
    kSFSDirectoryQuery_PathComponent,
    kSFSDirectoryQuery_InodeId
};

typedef struct SFSDirectoryQuery {
    int     kind;
    union _u {
        const PathComponent* _Nonnull   pc;
        InodeId                         id;
    }       u;
} SFSDirectoryQuery;



//
// Inode Extensions
//

#define Inode_GetBlockMap(__self) \
    Inode_GetRefConAs(__self, SFSBlockMap*)


//
// SerenaFS
//

CLASS_IVARS(SerenaFS, Filesystem,
    Lock                    lock;                           // Shared between filesystem proper and inodes
    ConditionVariable       notifier;

    DiskDriverRef _Nullable diskDriver;
    
    LogicalBlockAddress     allocationBitmapLba;            // Info for writing the allocation bitmap back to disk
    LogicalBlockCount       allocationBitmapBlockCount;     // -"-
    uint8_t* _Nullable      allocationBitmap;
    size_t                  allocationBitmapByteSize;
    uint32_t                volumeBlockCount;

    LogicalBlockAddress     rootDirLba;                     // Root directory LBA (This is the inode id at the same time)

    bool                    isReadOnly;                     // true if mounted read-only; false if mounted read-write
    uint8_t                 tmpBlock[kSFSBlockSize];
);

typedef ssize_t (*SFSReadCallback)(void* _Nonnull pDst, const void* _Nonnull pSrc, ssize_t n);
typedef void (*SFSWriteCallback)(void* _Nonnull pDst, const void* _Nonnull pSrc, ssize_t n);

typedef enum SFSBlockMode {
    kSFSBlockMode_Read = 0,
    kSFSBlockMode_Write
} SFSBlockMode;


static InodeId SerenaFS_GetNextAvailableInodeId_Locked(SerenaFSRef _Nonnull self);
static errno_t SerenaFS_FormatWithEmptyFilesystem(SerenaFSRef _Nonnull self);
static errno_t SerenaFS_CreateDirectoryDiskNode(SerenaFSRef _Nonnull self, InodeId parentId, UserId uid, GroupId gid, FilePermissions permissions, InodeId* _Nonnull pOutId);
static void SerenaFS_DestroyDiskNode(SerenaFSRef _Nonnull self, SFSInodeRef _Nullable pDiskNode);
static errno_t SerenaFS_GetLogicalBlockAddressForFileBlockAddress(SerenaFSRef _Nonnull self, InodeRef _Nonnull pNode, int fba, SFSBlockMode mode, LogicalBlockAddress* _Nonnull pOutLba);
static void SerenaFS_xTruncateFile(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset length);

#endif /* SerenaFSPriv_h */

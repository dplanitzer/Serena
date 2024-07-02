//
//  VolumeFormat.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/7/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef VolumeFormat_h
#define VolumeFormat_h

#include <klib/Types.h>

#define kSFSMaxFilenameLength               28
#define kSFSBlockSizeShift                  9
#define kSFSBlockSize                       (1 << kSFSBlockSizeShift)
#define kSFSBlockSizeMask                   (kSFSBlockSize - 1)
#define kSFSDirectoryEntriesPerBlock        (kSFSBlockSize / sizeof(SFSDirectoryEntry))
#define kSFSDirectoryEntriesPerBlockMask    (kSFSDirectoryEntriesPerBlock - 1)
#define kSFSDirectBlockPointersCount        113
#define kSFSIndirectBlockPointersCount      1
#define kSFSInodeBlockPointersCount         (kSFSDirectBlockPointersCount + kSFSIndirectBlockPointersCount)
#define kSFSBlockPointersPerBlockCount      128


//
// Serena FS Volume Format
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
    kSFSVersion_v0_1 = 0x00000100,              // v0.1.0
    kSFSVersion_v1_0 = 0x00010000,              // v1.0.0
    kSFSVersion_Current = kSFSVersion_v0_1,     // Version to use for formatting a new disk
};

enum {
    kSFSVolumeAttributeBit_ReadOnly = 0,        // If set then the volume is (software) write protected. A volume is R/W-able if it is neither software nor hardware read-only
    kSFSVolumeAttributeBit_IsConsistent = 1,    // OnMount() must clear this bit on the disk and onUnmount must set it on disk as the last write operation. If this bit is cleared on mount then the FS state on disk should be considered inconsistent
};

typedef struct SFSDateTime {
    uint32_t    tv_sec;     // Seconds since 00:00:00 UTC Jan 1st, 1970
    uint32_t    tv_nsec;    // 0..<1billion
} SFSDateTime;


typedef struct SFSVolumeHeader {
    uint32_t    signature;
    uint32_t    version;
    uint32_t    attributes;

    SFSDateTime creationTime;               // Date/time when the disk was formatted to create the FS
    SFSDateTime modificationTime;           // Date/time when the most recent modification to the FS happened
    
    uint32_t    blockSize;                  // Allocation block size (currently always == disk block size)
    uint32_t    volumeBlockCount;           // Size of the volume in terms of allocation blocks
    uint32_t    allocationBitmapByteSize;   // Size of allocation bitmap in bytes (XXX tmp until we'll turn the allocation bitmap into a real file)

    uint32_t    rootDirectoryLba;           // LBA of the root directory Inode
    uint32_t    allocationBitmapLba;        // LBA of the first block of the allocation bitmap area
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

// XXX currently limited to file sizes of 122k. That's fine for now since we'll
// XXX move to B-Trees for block mapping, directory content and extended
// XXX attributes anyway.
typedef uint32_t SFSBlockNumber;

typedef struct SFSInode {
    SFSDateTime     accessTime;
    SFSDateTime     modificationTime;
    SFSDateTime     statusChangeTime;
    int64_t         size;
    uint32_t        uid;
    uint32_t        gid;
    int32_t         linkCount;
    uint16_t        permissions;
    uint8_t         type;
    uint8_t         reserved;
    SFSBlockNumber  bp[kSFSInodeBlockPointersCount];
} SFSInode;
typedef SFSInode* SFSInodeRef;


#define kSFSLimit_LinkMax INT32_MAX         /* Max number of hard links to a directory/file */
#define kSFSLimit_FileSizeMax INT64_MAX     /* Permissible max size of a file in terms of bytes */


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
    uint32_t    id;
    char        filename[kSFSMaxFilenameLength];    // if strlen(filename) < kSFSMaxFilenameLength -> 0 terminated
} SFSDirectoryEntry;

#endif /* VolumeFormat_h */

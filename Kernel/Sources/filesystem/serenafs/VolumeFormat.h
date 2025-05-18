//
//  VolumeFormat.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/7/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef VolumeFormat_h
#define VolumeFormat_h

#include <kern/types.h>


//
// Serena FS Volume Format
//

// Meaning of 'reserved' bytes:
// * Ignore on read
// * Set to 0 when formatting
// * Do not modify on write (preserve whatever values the reserved bytes have)

typedef uint32_t sfs_bno_t;     // logical block number
typedef uint32_t sfs_mode_t;    // file type and permissions. Keep aligned with mode_t


enum {
    kSFSMaxFilenameLength = 27,
    kSFSMaxVolumeLabelLength = 48,
    kSFSDirectBlockPointersCount = 110,
};

enum {
    kSFSSignature_SerenaFS = 0x53654653,        // 'SeFS'
    kSFSSignature_Inode = 0x6e6f6465,           // 'node'
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
    kSFSVolume_HeaderBno = 0,
    kSFSVolume_MinBlockSize = 512,
    kSFSVolume_MinBlockCount = 4,   // Need space for at least 1 volume header block + 1 allocation bitmap block + 1 root dir inode + 1 root dir content block
};

#define kSFSLimit_LinkMax INT32_MAX         /* Max number of hard links to a directory/file */
#define kSFSLimit_FileSizeMax INT64_MAX     /* Permissible max size of a file in terms of bytes */


//
// Volume Header
//
// The volume header is stored in logical block #0 on the disk. It stores data
// to identify the filesystem type and version and to locate the root directory
// and other files needed to manage the filesystem.

enum {
    kSFSVolAttrib_ReadOnly = 1,         // If set then the volume is (software) write protected. A volume is R/W-able if it is neither software nor hardware read-only
    kSFSVolAttrib_IsConsistent = 2,     // Start() must clear this bit on the disk and onUnmount must set it on disk as the last write operation. If this bit is cleared on mount then the FS state on disk should be considered inconsistent
};

typedef struct sfs_datetime {
    uint32_t    tv_sec;     // Seconds since 00:00:00 UTC Jan 1st, 1970
    uint32_t    tv_nsec;    // 0..<1billion
} sfs_datetime_t;


typedef struct sfs_vol_header {
    uint32_t        signature;                  // kSFSSignature_SerenaFS
    uint32_t        version;
    uint32_t        attributes;

    sfs_datetime_t  creationTime;               // Date/time when the disk was formatted to create the FS
    sfs_datetime_t  modificationTime;           // Date/time when the most recent modification to the FS happened
    
    uint32_t        volBlockSize;               // Volume block size (currently always == disk block size)
    uint32_t        volBlockCount;              // Size of the volume in terms of volume blocks
    uint32_t        allocBitmapByteSize;        // Size of allocation bitmap in bytes

    sfs_bno_t       lbaRootDir;                 // LBA of the root directory Inode
    sfs_bno_t       lbaAllocBitmap;             // LBA of the first block of the allocation bitmap area

    uint8_t         labelLength;
    char            label[kSFSMaxVolumeLabelLength];    // Volume label string
    // All bytes from here to the end of the block are reserved
} sfs_vol_header_t;


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
// Block Map
//

typedef struct sfs_bmap {
    sfs_bno_t   indirect;
    sfs_bno_t   direct[kSFSDirectBlockPointersCount];
} sfs_bmap_t;


//
// Inodes
//
// XXX currently limited to file sizes of 122k. That's fine for now since we'll
// XXX move to B-Trees for block mapping, directory content and extended
// XXX attributes anyway.
typedef struct sfs_inode {
    int64_t         size;
    sfs_datetime_t  accessTime;
    sfs_datetime_t  modificationTime;
    sfs_datetime_t  statusChangeTime;
    sfs_bno_t       id;                     // Id (lba) of this inode
    sfs_bno_t       pnid;                   // Id (lba) of the parent inode (directory)
    uint32_t        signature;              // kSFSSignature_Inode
    int32_t         linkCount;
    sfs_mode_t      mode;
    uint32_t        uid;
    uint32_t        gid;
    sfs_bmap_t      bmap;
} sfs_inode_t;


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
// A directory file is organized into pages. Each page corresponds exactly to a
// filesystem block. A page stores an array of sfs_dirent data structures.
//
// Internal directory organisation:
// [0] "."
// [1] ".."
// [2] userEntry0
// .
// [n] userEntryN-1
// This must be mod(SFSDiskBlockSize, SFSDirectoryEntrySize) == 0
// The number of entries in the directory file is fileLength / sizeof(sfs_dirent_t)
//
// File names are stored without a trailing NUL character since the length is
// explicitly stored in 'len'.
//
// The '.' and '..' entries of the root directory map to the root directory
// inode id.
typedef struct sfs_dirent {
    uint32_t    id;
    uint8_t     len;
    char        filename[kSFSMaxFilenameLength];    // if strlen(filename) < kSFSMaxFilenameLength
} sfs_dirent_t;

#endif /* VolumeFormat_h */

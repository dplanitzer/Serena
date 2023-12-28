//
//  _kbidef.h
//  Apollo
//
//  Created by Dietmar Planitzer on 9/14/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __KBIDEF_H
#define __KBIDEF_H 1

#include <_nulldef.h>
#include <_dmdef.h>
#include <_syslimits.h>

// The process arguments descriptor is stored in the process address space and
// it contains a pointer to the base of the command line arguments and environment
// variables tables. These tables store pointers to nul-terminated strings and
// the last entry in the table contains a NULL.
// This data structure is set up by the kernel when it processes an exec() or
// spawn() request. Once set up the kernel neither reads nor writes to this area.
struct __process_arguments_t {
    __size_t                    version;    // sizeof(__process_arguments_t)
    __size_t                    reserved;
    __size_t                    arguments_size; // Size of the area that holds all of __process_arguments_t + argv + envp
    __size_t                    argc;           // Number of command line arguments passed to the process. Argv[0] holds the path to the process through which it was started
    char* _Nullable * _Nonnull  argv;           // Pointer to the base of the command line arguments table. Last entry is NULL
    char* _Nullable * _Nonnull  envp;           // Pointer to the base of the environment table. Last entry holds NULL.
    void* _Nonnull              image_base;     // Pointer to the base of the executable header
};


// Child process should not inherit the default descriptors. The default
// descriptors are the parent process' stdin, stdout and stderr descriptors.
#define SPAWN_NO_DEFAULT_DESCRIPTOR_INHERITANCE 0x0001

// Instructs the spawn() call to set the umask of the newly spawned process to
// the umask field in the spawn arguments struct rather than the umask field of
// the parent process.
#define SPAWN_OVERRIDE_UMASK 0x0002

// The 'envp' pointer points to a table of nul-terminated strings of the form
// 'key=value'. The last entry in the table has to be NULL. All these strings
// are the enviornment variables that should be passed to the new process.
// Both 'argv' and 'envp' may be NULL pointers. A NULL pointer is equivalent to
// a table with a single entry that is the NULL pointer. So a NULL 'argv'
// pointer means that the child process receives no command line arguments and
// a NULL 'envp' means that the child process receives an empty environment.
// If different semantics is desired then this must be implemented by the user
// space side of the system call. The recommended semantics for argv is that
// a NULL pointer is equivalent to { 'path', NULL } and for envp a NULL pointer
// should be substituted with the contents of the 'environ' variable.
struct __spawn_arguments_t {
    void* _Nonnull                      execbase;
    const char* _Nullable * _Nullable   argv;
    const char* _Nullable * _Nullable   envp;
    const char* _Nullable               root_dir;       // Process root directory, if not NULL; otherwise inherited from the parent
    const char* _Nullable               cw_dir;         // Process current working directory, if not NULL; otherwise inherited from the parent
    unsigned short                      umask;          // Override umask
    unsigned int                        options;
};


// The result of a waitpid system call.
struct __waitpid_result_t {
    int/*pid_t*/    pid;        // PID of the child process
    int             status;     // Child process exit status
};


struct _time_interval_t {
    long    seconds;
    long    nanoseconds;        // 0..<1billion
};


typedef long long _file_offset_t;
typedef unsigned short _file_permissions_t;
typedef signed char _file_type_t;
typedef long _filesystem_id;
typedef long _inode_id;
typedef unsigned long _uid_t;
typedef unsigned long _gid_t;

struct _file_info_t {
    struct _time_interval_t accessTime;
    struct _time_interval_t modificationTime;
    struct _time_interval_t statusChangeTime;
    _file_offset_t          size;
    _uid_t                  uid;
    _gid_t                  gid;
    _file_permissions_t     permissions;
    _file_type_t            type;
    char                    reserved;
    long                    linkCount;
    _filesystem_id          filesystemId;
    _inode_id               inodeId;
};

enum {
    kModifyFileInfo_AccessTime = 1,
    kModifyFileInfo_ModificationTime = 2,
    kModifyFileInfo_UserId = 4,
    kModifyFileInfo_GroupId = 8,
    kModifyFileInfo_Permissions = 16
};

struct _mutable_file_info_t {
    unsigned long           modify;
    struct _time_interval_t accessTime;
    struct _time_interval_t modificationTime;
    _uid_t                  uid;
    _gid_t                  gid;
    _file_permissions_t     permissions;
    unsigned short          permissionsModifyMask;  // Only modify permissions whose bit is set here
};


struct _directory_entry_t {
    _inode_id   inodeId;
    char        name[__PATH_COMPONENT_MAX];
};


enum {
    kAccess_Readable = 1,
    kAccess_Writable = 2,
    kAccess_Executable = 4,
    kAccess_Searchable = kAccess_Executable,    // For directories
    kAccess_Exists = 0
};


#define IOResourceCommand(__cmd) (__cmd)
#define IOChannelCommand(__cmd) -(__cmd)
#define IsIOChannelCommand(__cmd) ((__cmd) < 0)


#define O_RDONLY    0x0001
#define O_WRONLY    0x0002
#define O_RDWR      (O_RDONLY | O_WRONLY)
#define O_APPEND    0x0004
#define O_EXCL      0x0008
#define O_TRUNC     0x0010

#endif /* __KBIDEF_H */

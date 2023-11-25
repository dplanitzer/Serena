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

#endif /* __KBIDEF_H */

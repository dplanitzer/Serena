//
//  info.c
//  cmds
//
//  Created by Dietmar Planitzer on 4/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <System/Error.h>
#include <System/File.h>
#include <System/Filesystem.h>
#include <System/Process.h>

#define DISKNAME_BUFSIZE    16


static errno_t print_cat_info(const FSInfo* _Nonnull info, int fd)
{
    decl_try_err();
    char diskName[32];

    try(IOChannel_Control(fd, kFSCommand_GetDiskName, sizeof(diskName), diskName));

    puts("Catalog ID");
    printf("%s       %u\n", diskName, info->fsid);

catch:
    return err;
}

static errno_t print_reg_info(const FSInfo* _Nonnull info, int fd)
{
    decl_try_err();
    const uint64_t size = (uint64_t)info->capacity * (uint64_t)info->blockSize;
    const unsigned fullPerc = info->count * 100 / info->capacity;
    const char* status;
    char diskName[32];
    char volLabel[64];

    try(IOChannel_Control(fd, kFSCommand_GetDiskName, sizeof(diskName), diskName));
    try(IOChannel_Control(fd, kFSCommand_GetLabel, sizeof(volLabel), volLabel));


    if ((info->properties & kFSProperty_IsReadOnly) == kFSProperty_IsReadOnly) {
        status = "Read Only";
    }
    else {
        status = "Read/Write";
    }


    // XX formatting
    puts("Disk ID Size   Used   Free Full Status Type Name");
    printf("%s %u %lluK %u %u %u%% %s %s %s\n", diskName, info->fsid, size / 1024ull, info->count, info->capacity - info->count, fullPerc, status, info->type, volLabel);

catch:
    return err;
}


static errno_t get_cwd_fsid(fsid_t* _Nonnull fsid)
{
    FileInfo info;
    errno_t err = 0;
    char* path = NULL;

    path = malloc(PATH_MAX);
    if (path == NULL) {
        return ENOMEM;
    }

    err = Process_GetWorkingDirectory(path, PATH_MAX);
    if (err == EOK) {
        err = File_GetInfo(path, &info);
        if (err == EOK) {
            *fsid = info.fsid;
        }
    }
    free(path);

    return err;
}

int main(int argc, char* argv[])
{
    decl_try_err();
    fsid_t fsid;
    FSInfo info;
    int fd = -1;
    char path[32];

    if (argc < 2 ) {
        try(get_cwd_fsid(&fsid));
    }
    else {
        errno = EOK;
        fsid = atoi(argv[1]);
        if (errno != EOK) {
            throw(errno);
        }
    }


    sprintf(path, "/fs/%u", fsid);
    try(File_Open(path, kOpen_Read, &fd));
    try(IOChannel_Control(fd, kFSCommand_GetInfo, &info));

    if ((info.properties & kFSProperty_IsCatalog) == kFSProperty_IsCatalog) {
        err = print_cat_info(&info, fd);
    }
    else {
        err = print_reg_info(&info, fd);
    }


catch:

    if (err != EOK) {
        printf("%s: %s\n", argv[0], strerror(err));
    }

    if (fd >= 0) {
        IOChannel_Close(fd);
    }

    return (err == EOK) ? EXIT_SUCCESS : EXIT_FAILURE;
}

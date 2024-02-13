//
//  FileIOTests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 7/9/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <System/System.h>


static void pwd(void)
{
    char buf[128];
    const errno_t err = Process_GetWorkingDirectory(buf, sizeof(buf));

    if (err == 0) {
        printf("cwd: \"%s\"\n", buf);
    } else {
        printf("pwd error: %s\n", strerror(err));
    }
}

static void chdir(const char* path)
{
    const errno_t err = Process_SetWorkingDirectory(path);

    if (err != 0) {
        printf("chdir error: %s\n", strerror(err));
    }
}

static void _mkdir(const char* path)
{
    const errno_t err = Directory_Create(path, 0777);
    if (err != 0) {
        printf("mkdir error: %s\n", strerror(err));
    }
}

static int _opendir(const char* path)
{
    int fd;
    const errno_t err = Directory_Open(path, &fd);
    if (err != 0) {
        printf("opendir error: %s\n", strerror(err));
    }
    return fd;
}

static void _read(int fd, void* buffer, size_t nbytes, ssize_t* nOutBytesRead)
{
    const errno_t err = IOChannel_Read(fd, buffer, nbytes, nOutBytesRead);

    if (err != 0) {
        printf("read error: %s\n", strerror(err));
    }
}

static void _write(int fd, const void* buffer, size_t nbytes, ssize_t* nOutBytesWritten)
{
    const errno_t err = IOChannel_Write(fd, buffer, nbytes, nOutBytesWritten);

    if (err != 0) {
        printf("write error: %s\n", strerror(err));
    }
}

static void _close(int fd)
{
    const errno_t err = IOChannel_Close(fd);

    if (err != 0) {
        printf("close error: %s\n", strerror(err));
    }
}


static void print_fileinfo(const char* path)
{
    FileInfo info;
    const errno_t err = File_GetInfo(path, &info);
    if (err != 0) {
        printf("File_GetInfo error: %s\n", strerror(err));
        return;
    }

    printf("Info for \"%s\":\n", path);
    printf("  size:   %lld\n", info.size);
    printf("  uid:    %lu\n", info.uid);
    printf("  gid:    %lu\n", info.gid);
    printf("  permissions: 0%ho\n", info.permissions);
    printf("  type:   %hhd\n", info.type);
    printf("  nlinks: %ld\n", info.linkCount);
    printf("  fsid:   %ld\n", info.filesystemId);
    printf("  inid:   %ld\n", info.inodeId);
}


////////////////////////////////////////////////////////////////////////////////
// Tests
////////////////////////////////////////////////////////////////////////////////

void chdir_pwd_test(int argc, char *argv[])
{
    printf("uid: %ld\n", Process_GetUserId());
    printf("umask: 0%o\n\n", Process_GetUserMask());

    _mkdir("/Users");
    _mkdir("/Users/Admin");
    _mkdir("/Users/Tester");

//while(true) {
    pwd();
    chdir("/Users");
    pwd();
    chdir("/Users/Admin");
    pwd();
    chdir("/Users/Tester");
    pwd();
    chdir("/Users");
    pwd();
    chdir("./Admin/.");
    pwd();
    chdir("../Tester");
    pwd();
//}
}

void fileinfo_test(int argc, char *argv[])
{
    _mkdir("/Users");
    _mkdir("/Users/Admin");
    _mkdir("/Users/Tester");

    print_fileinfo("/Users");
    printf("\n");
    print_fileinfo("/Users/Admin");
}

void unlink_test(int argc, char *argv[])
{
    _mkdir("/Users");
    _mkdir("/Users/Admin");
    _mkdir("/Users/Tester");

    File_Unlink("/Users/Tester");
}

void readdir_test(int argc, char *argv[])
{
    _mkdir("/Users");
    _mkdir("/Users/Admin");
    _mkdir("/Users/Tester");

    const int fd = _opendir("/Users");
    //const int fd = _opendir("/");
    DirectoryEntry dirent;

    while (true) {
        ssize_t r;
        _read(fd, &dirent, sizeof(dirent), &r);
        if (r == 0) {
            break;
        }

        printf("%ld:\t\"%s\"\n", dirent.inodeId, dirent.name);
    }
    _close(fd);
}

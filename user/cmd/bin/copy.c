//
//  copy.c
//  cmds
//
//  Created by Dietmar Planitzer on 5/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <serena/file.h>
#include <serena/process.h>


#define COPY_BUF_SIZE   8192

char path_buf[PATH_MAX];
char copy_buf[COPY_BUF_SIZE];


const char* src_path = "";
const char* dst_path = "";

CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("copy <src_path> <dst_path>"),

    CLAP_REQUIRED_POSITIONAL_STRING(&src_path, "expected a path to an existing file"),
    CLAP_REQUIRED_POSITIONAL_STRING(&dst_path, "expected a path to the destination")
);


static int copy_file_contents(int sfd, int dfd)
{
    for (;;) {
        const ssize_t nBytesRead = fd_read(sfd, copy_buf, COPY_BUF_SIZE);
        if (nBytesRead <= 0) {
            return (nBytesRead == 0) ? 0 : -1;
        }

        const nBytesWritten = fd_write(dfd, copy_buf, nBytesRead);
        if (nBytesWritten != nBytesRead) {
            return -1;
        }
    }

    return 0;
}

static int copy_file(const char* _Nonnull src_path, const fs_attr_t* _Nonnull src_attr, const char* _Nonnull dst_path)
{
    int sfd = -1, dfd = -1;
    fs_perms_t fsperms = src_attr->permissions;

    // Need to ensure that the destination file has write permissions so that we
    // can actually copy the data
    fs_perms_add(fsperms, FS_PCLS_USR, FS_PRM_W);


    sfd = open(src_path, O_RDONLY);
    if (sfd >= 0) {
        dfd = open(dst_path, O_CREAT|O_EXCL|O_WRONLY, fsperms);
    }

    if (sfd != -1 && dfd != -1) {
        if (copy_file_contents(sfd, dfd) != 0) {
            //XXX a funlink() would be nice here...
            fs_remove(NULL, dst_path);
            fd_close(dfd);
            dfd = -1;
        }
    }


    // Remove the write rights from the destination if the source doesn't have
    // write rights
    if (dfd != -1 && !fs_perms_has(src_attr->permissions, FS_PCLS_USR, FS_PRM_W)) {
        //XXX use fchmod() instead once it exists
        fs_setperms(NULL, dst_path, src_attr->permissions);
    }


    if (dfd != -1) {
        fd_close(dfd);
    }
    if (sfd != -1) {
        fd_close(sfd);
    }

    return (sfd != -1 && dfd != -1) ? 0 : -1;
}

static const char* _Nullable make_path(const char* _Nonnull dirPath, const char* _Nonnull lastPathComponent)
{
    const size_t dstPathLen = strlen(dirPath);
    const size_t lastPathComponentLen = strlen(lastPathComponent);
    const size_t hasSlash = (dirPath[dstPathLen - 1] == '/') ? 1 : 0;
    const size_t pathLen = dstPathLen + hasSlash + lastPathComponentLen;
    char* p = path_buf;

    if (pathLen > PATH_MAX - 1) {
        errno = EINVAL;
        return NULL;
    }

    memcpy(p, dirPath, dstPathLen);
    p += dstPathLen;
    if (!hasSlash) {
        *p++ = '/';
    }
    memcpy(p, lastPathComponent, lastPathComponentLen);
    
    return path_buf;
}

static int copy_obj(const char* _Nonnull src_path, const char* _Nonnull dst_path)
{
    fs_attr_t src_attr, dst_attr;
    bool hasSrc = false, hasDst = false;

    if (fs_attr(NULL, src_path, &src_attr) == 0) {
        hasSrc = true;
    }
    if (fs_attr(NULL, dst_path, &dst_attr) == 0) {
        hasDst = true;
    }


    // Source must be a file for now
    if (!hasSrc || (src_attr.file_type != FS_FTYPE_REG)) {
        errno = EINVAL;
        return -1;
    }

    // Destination must not exist or not be a directory
    if (hasDst && (dst_attr.file_type != FS_FTYPE_DIR)) {
        errno = EINVAL;
        return -1;
    }


    // Compute the final destination path if 'dst_path' points to a directory
    if (hasDst && (dst_attr.file_type == FS_FTYPE_DIR)) {
        const char* lastSrcPathComponent = strrchr(src_path, '/');
        const char* fileName = (lastSrcPathComponent) ? lastSrcPathComponent + 1 : src_path;

        dst_path = make_path(dst_path, fileName);
    }


    return copy_file(src_path, &src_attr, dst_path);
}


int main(int argc, char* argv[])
{
    clap_parse(0, params, argc, argv);

    if (copy_obj(src_path, dst_path) == 0) {
        return EXIT_SUCCESS;
    }
    else {
        clap_error(argv[0], "%s: %s", src_path, strerror(errno));
        return EXIT_FAILURE;
    }
}

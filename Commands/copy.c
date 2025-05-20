//
//  copy.c
//  cmds
//
//  Created by Dietmar Planitzer on 5/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/perm.h>
#include <sys/stat.h>


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
    ssize_t nBytesRead, nBytesWritten;
    int err;

    for (;;) {
        err = read(sfd, copy_buf, COPY_BUF_SIZE, &nBytesRead);
        if (err != 0 || nBytesRead == 0) {
            return (err == 0) ? 0 : -1;
        }

        err = write(dfd, copy_buf, nBytesRead, &nBytesWritten);
        if (err != 0) {
            return -1;
        }
    }

    return 0;
}

static int copy_file(const char* _Nonnull srcPath, const struct stat* _Nonnull srcStat, const char* _Nonnull dstPath)
{
    int sfd = -1, dfd = -1;
    mode_t perms = S_FPERM(srcStat->st_mode);
    int err;

    // Need to ensure that the destination file has write permissions so that we
    // can actually copy the data
    perm_add(perms, S_ICUSR, S_IWRITE);


    err = open(srcPath, O_RDONLY, &sfd);
    if (err == 0) {
        err = creat(dstPath, O_WRONLY|O_TRUNC, perms, &dfd);
    }

    if (sfd != -1 && dfd != -1) {
        if (copy_file_contents(sfd, dfd) != 0) {
            //XXX a funlink() would be nice here...
            unlink(dstPath);
            close(dfd);
            dfd = -1;
        }
    }


    // Remove the write rights from the destination if the source doesn't have
    // write rights
    if (dfd != -1 && !perm_has(srcStat->st_mode, S_ICUSR, S_IWRITE)) {
        //XXX use fchmod() instead once it exists
        chmod(dstPath, srcStat->st_mode);
    }


    if (dfd != -1) {
        close(dfd);
    }
    if (sfd != -1) {
        close(sfd);
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

static int copy_obj(const char* _Nonnull srcPath, const char* _Nonnull dstPath)
{
    struct stat srcstat, dststat;
    bool hasSrc = false, hasDst = false;

    if (stat(srcPath, &srcstat) == 0) {
        hasSrc = true;
    }
    if (stat(dstPath, &dststat) == 0) {
        hasDst = true;
    }


    // Source must be a file for now
    if (!hasSrc || !S_ISREG(srcstat.st_mode)) {
        errno = EINVAL;
        return -1;
    }

    // Destination must not exist or be a directory
    if (hasDst && !S_ISDIR(dststat.st_mode)) {
        errno = EINVAL;
        return -1;
    }


    // Compute the final destination path if 'dstPath' points to a directory
    if (hasDst && S_ISDIR(dststat.st_mode)) {
        const char* lastSrcPathComponent = strrchr(srcPath, '/');
        const char* fileName = (lastSrcPathComponent) ? lastSrcPathComponent + 1 : srcPath;

        dstPath = make_path(dstPath, fileName);
    }


    return copy_file(srcPath, &srcstat, dstPath);
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

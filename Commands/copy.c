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
#include <fcntl.h>
#include <sys/stat.h>


const char* src_path = "";
const char* dst_path = "";

CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("copy <src_path> <dst_path>"),

    CLAP_REQUIRED_POSITIONAL_STRING(&src_path, "expected a path to an existing file"),
    CLAP_REQUIRED_POSITIONAL_STRING(&dst_path, "expected a path to the destination")
);


static errno_t _copy_file(const char* _Nonnull srcPath, const char* _Nonnull dstPath)
{
    decl_try_err();
    const size_t bufSize = 1024;
    char* buf = NULL;
    int sfd = -1, dfd = -1;
    finfo_t finf;
    FilePermissions perms;
    ssize_t nBytesRead, nBytesWritten;

    try_null(buf, malloc(bufSize), ENOMEM);


    try(open(srcPath, O_RDONLY, &sfd));
    try(fgetfinfo(sfd, &finf));
    perms = finf.permissions;
    FilePermissions_Add(perms, kFilePermissionsClass_User, kFilePermission_Write);
    try(creat(dstPath, O_WRONLY|O_TRUNC, perms, &dfd));

    while (err == EOK) {
        err = read(sfd, buf, bufSize, &nBytesRead);
        if (nBytesRead == 0 || err != EOK) {
            break;
        }

        err = write(dfd, buf, nBytesRead, &nBytesWritten);
    }

    if (!FilePermissions_Has(finf.permissions, kFilePermissionsClass_User, kFilePermission_Write)) {
        fmutinfo_t mfi;

        mfi.modify = kModifyFileInfo_Permissions;
        mfi.permissions = finf.permissions;
        mfi.permissionsModifyMask = UINT16_MAX;
        fsetfinfo(dfd, &mfi);
    }


catch:
    if (dfd != -1) {
        close(dfd);
    }
    if (sfd != -1) {
        close(sfd);
    }
    free(buf);
    return err;
}

static errno_t copy_file(const char* _Nonnull srcPath, const char* _Nonnull dstPath)
{
    decl_try_err();
    finfo_t finf;
    char* dpath;

    err = getfinfo(srcPath, &finf);
    if (err == EOK) {
        if (finf.type != S_IFREG) {
            return EINVAL;
        }
    }
    else {
        return err;
    }


    err = getfinfo(dstPath, &finf);
    if (err == EOK) {
        if (finf.type == S_IFDIR) {
            dpath = malloc(PATH_MAX);
            if (dpath == NULL) {
                return ENOMEM;
            }

            const size_t dstPathLen = strlen(dstPath);
            const char* lastSrcPathComponent = strrchr(srcPath, '/');
            const char* fname = (lastSrcPathComponent) ? lastSrcPathComponent + 1 : srcPath;
            const size_t fnameLen = strlen(fname);
            const size_t hasSlash = (dstPath[dstPathLen - 1] == '/') ? 1 : 0;
            const size_t dpathLen = dstPathLen + hasSlash + fnameLen;
            char* p = dpath;

            if (dpathLen > PATH_MAX - 1) {
                return EINVAL;
            }

            memcpy(p, dstPath, dstPathLen);
            p += dstPathLen;
            if (!hasSlash) {
                *p++ = '/';
            }
            memcpy(p, fname, fnameLen);
        }
        else if (finf.type == S_IFREG) {
            dpath = (char*)dstPath;
        }
        else {
            return EINVAL;
        }
    }
    else {
        dpath = (char*)dstPath;
    }

    
    err = _copy_file(srcPath, dpath);
    if (dpath != dstPath) {
        free(dpath);
    }

    return err;
}


int main(int argc, char* argv[])
{
    decl_try_err();

    clap_parse(0, params, argc, argv);

    
    err = copy_file(src_path, dst_path);
    if (err != EOK) {
        clap_error(argv[0], "%s: %s", src_path, strerror(err));
    }

    return (err == EOK) ? EXIT_SUCCESS : EXIT_FAILURE;
}

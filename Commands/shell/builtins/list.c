//
//  list.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "cmdlib.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <clap.h>
#include <System/_math.h>


#if defined(__ILP32__)
#define PINID   PRIu32
#elif defined(__LP64__) || defined(__LLP64__)
#define PINID   PRIu64
#else
#error "Unknown data model"
#endif

typedef struct ListContext {
    char*   pathBuffer;

    int     linkCountWidth;
    int     uidWidth;
    int     gidWidth;
    int     sizeWidth;
    int     inodeIdWidth;

    struct Flags {
        unsigned int printAll:1;
        unsigned int reserved:31;
    }       flags;
} ListContext;
typedef ListContext* ListContextRef;


typedef errno_t (*DirectoryIteratorCallback)(ListContextRef _Nonnull self, const char* _Nonnull pDirPath, DirectoryEntry* _Nonnull pEntry);


static void file_permissions_to_text(FilePermissions perms, char* _Nonnull buf)
{
    if ((perms & kFilePermission_Read) == kFilePermission_Read) {
        buf[0] = 'r';
    }
    if ((perms & kFilePermission_Write) == kFilePermission_Write) {
        buf[1] = 'w';
    }
    if ((perms & kFilePermission_Execute) == kFilePermission_Execute) {
        buf[2] = 'x';
    }
}

static errno_t calc_dir_entry_format(ListContextRef _Nonnull self, const char* _Nonnull pDirPath, DirectoryEntry* _Nonnull pEntry)
{
    decl_try_err();
    FileInfo info;

    strcpy(self->pathBuffer, pDirPath);
    strcat(self->pathBuffer, "/");
    strcat(self->pathBuffer, pEntry->name);

    try(File_GetInfo(self->pathBuffer, &info));

    itoa(info.linkCount, self->pathBuffer, 10);
    self->linkCountWidth = __max(self->linkCountWidth, strlen(self->pathBuffer));
    itoa(info.uid, self->pathBuffer, 10);
    self->uidWidth = __max(self->uidWidth, strlen(self->pathBuffer));
    itoa(info.gid, self->pathBuffer, 10);
    self->gidWidth = __max(self->gidWidth, strlen(self->pathBuffer));
    lltoa(info.size, self->pathBuffer, 10);
    self->sizeWidth = __max(self->sizeWidth, strlen(self->pathBuffer));
    itoa(info.inodeId, self->pathBuffer, 10);
    self->inodeIdWidth = __max(self->inodeIdWidth, strlen(self->pathBuffer));

catch:
    return err;
}

static errno_t print_dir_entry(ListContextRef _Nonnull self, const char* _Nonnull pDirPath, DirectoryEntry* _Nonnull pEntry)
{
    decl_try_err();
    FileInfo info;

    strcpy(self->pathBuffer, pDirPath);
    strcat(self->pathBuffer, "/");
    strcat(self->pathBuffer, pEntry->name);

    try(File_GetInfo(self->pathBuffer, &info));

    char tp[11];
    for (int i = 0; i < sizeof(tp); i++) {
        tp[i] = '-';
    }
    if (info.type == kFileType_Directory) {
        tp[0] = 'd';
    }
    file_permissions_to_text(FilePermissions_Get(info.permissions, kFilePermissionsClass_User), &tp[1]);
    file_permissions_to_text(FilePermissions_Get(info.permissions, kFilePermissionsClass_Group), &tp[4]);
    file_permissions_to_text(FilePermissions_Get(info.permissions, kFilePermissionsClass_Other), &tp[7]);
    tp[10] = '\0';

    printf("%s %*d  %*u %*u  %*lld %*" PINID " %s\n",
        tp,
        self->linkCountWidth, info.linkCount,
        self->uidWidth, info.uid,
        self->gidWidth, info.gid,
        self->sizeWidth, info.size,
        self->inodeIdWidth, info.inodeId,
        pEntry->name);
    
catch:
    return err;
}

static errno_t iterate_dir(ListContextRef _Nonnull self, int dp, const char* _Nonnull path, DirectoryIteratorCallback _Nonnull cb)
{
    decl_try_err();
    DirectoryEntry dirent;
    ssize_t r;

    while (true) {
        try(Directory_Read(dp, &dirent, 1, &r));
        if (r == 0) {
            break;
        }

        if (!self->flags.printAll && dirent.name[0] == '.') {
            continue;
        }

        try(cb(self, path, &dirent));
    }

catch:
    return err;
}

static errno_t list_dir(ListContextRef _Nonnull self, const char* _Nonnull path)
{
    decl_try_err();
    int dp;

    try(Directory_Open(path, &dp));
    try(iterate_dir(self, dp, path, calc_dir_entry_format));
    try(Directory_Rewind(dp));
    try(iterate_dir(self, dp, path, print_dir_entry));

catch:
    IOChannel_Close(dp);
    return err;
}


////////////////////////////////////////////////////////////////////////////////

static const char* default_path[1] = {"."};
static clap_string_array_t paths = {default_path, 1};
static bool is_print_all = false;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("list [-a | --all] <path>"),

    CLAP_BOOL('a', "all", &is_print_all, "Print entries starting with a '.'"),
    CLAP_VARARG(&paths)
);


int cmd_list(ShellContextRef _Nonnull pContext, int argc, char** argv, char** envp)
{
    decl_try_err();
    ListContext ctx = {0};
    bool anyError = false;

    paths.strings = default_path;
    paths.count = 1;
    is_print_all = false;

    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    if (clap_should_exit(status)) {
        return clap_exit_code(status);
    }

    ctx.flags.printAll = is_print_all;

    
    ctx.pathBuffer = malloc(PATH_MAX);
    if (ctx.pathBuffer == NULL) {
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < paths.count; i++) {
        const char* path = paths.strings[i];

        if (paths.count > 1) {
            printf("%s:\n", path);
        }

        err = list_dir(&ctx, path);
        if (err != EOK) {
            print_error(argv[0], path, err);
            anyError = true;
        }

        if (i < (argc - 1)) {
            putchar('\n');
        }
    }
    free(ctx.pathBuffer);

    return (anyError) ? EXIT_FAILURE : EXIT_SUCCESS;
}

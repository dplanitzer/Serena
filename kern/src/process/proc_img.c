//
//  proc_img.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/21/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "proc_img.h"
#include "ldr_gemdos.h"
#include "ldr_script.h"
#include <ext/math.h>
#include <ext/string.h>
#include <filemanager/FileManager.h>
#include <filesystem/IOChannel.h>
#include <kei/kei.h>
#include <kern/kalloc.h>
#include <kpi/fd.h>
#include <kpi/syslimits.h>
#include <machine/cpu.h>


typedef struct ctx_table {
    ssize_t                     tb_strings_size;    // Size of arg_strings in terms of bytes. Includes the trailing '\0'
    char* _Nonnull              tb_strings;         // Consecutive list of NUL-terminated process argument strings. End is marked by an empty string  
    size_t                      tbc;                // Number of command line arguments passed to the process. Argv[0] holds the path to the process through which it was started
    char* _Nullable * _Nonnull  tbv;                // Pointer to the base of the command line arguments table. Last entry is NULL
} ctx_table_t;

static void _proc_img_close_file(proc_img_t* _Nonnull pimg);


typedef errno_t (*ldr_func_t)(proc_img_t* _Nonnull pimg);
#define LDR_COUNT 2

static const ldr_func_t ldr_table[LDR_COUNT] = {
    ldr_script_load,
    ldr_gemdos_load
};


// Calculates the size of the string array that will be used to store the 'argv'
// strings. This includes the NUL marking the end of each string and the final
// NUL entry that marks the end of the string array. Note this is not the argv[]
// vector that points to the string. This is a separate thing.
static errno_t _get_table_size(const char* const _Nullable tb[], ctx_table_t* _Nonnull ctb)
{
    ssize_t nbytes = 0;
    size_t count = 0;

    while (tb[count]) {
        const char* p = tb[count];
        const ssize_t slen = strnlen_s(p, __ARG_STRLEN_MAX);
        const ssize_t entry_size = slen + 1;

        if (p[slen] != '\0' || (nbytes + entry_size) > __ARG_MAX) {
            return E2BIG;
        }

        nbytes += entry_size;
        count++;
    }
    ctb->tbc = count;
    ctb->tb_strings_size = nbytes + 1;   // +1 is for the empty string ('\0') that marks the end of the string array

    return EOK;
}

static void _copyin_table(const char* tb[], const ctx_table_t* _Nonnull ctb)
{
    char** dst_tbv = ctb->tbv;
    char* p = ctb->tb_strings;

    for (size_t i = 0; i < ctb->tbc; i++) {
        dst_tbv[i] = p;
        p = strcpy_x(p, tb[i]) + 1;
    }
    *p = '\0';
    dst_tbv[ctb->tbc] = NULL;
}

static void _copyin_aux_array(proc_aux_entry_t* _Nonnull aux, void* _Nonnull exec_hdr)
{
    aux[0].type = AT_EXEC_HDR;
    aux[0].u.p = exec_hdr;

    aux[1].type = AT_KEI;
    aux[1].u.p = gKeiTable;
    
    aux[2].type = AT_END;
    aux[2].u.p = NULL;
}

static errno_t _build_ctx(proc_img_t* _Nonnull pimg, const char* _Nullable argv[], const char* _Nullable env[])
{
    decl_try_err();
    static const char* null_sptr[1] = {NULL};

    if (argv == NULL) {
        argv = null_sptr;
    }
    if (env == NULL) {
        env = null_sptr;
    }


    // Build the proc_ctx_t region. Data is laid out like this to ensure proper
    // alignment:
    // proc_ctx_t, argv_table, envv_table, aux_array, arg_strings, env_strings
    //
    // Layout of the aux entries:
    // exec_hdr, kei_ptr, aux_end
    ctx_table_t dst_argv;
    ctx_table_t dst_env;
    proc_aux_entry_t* aux_array;

    try(_get_table_size(argv, &dst_argv));
    try(_get_table_size(env, &dst_env));

#define AUX_ENTRY_COUNT 3
    proc_ctx_t* pctx = NULL;
    const size_t argv_size = sizeof(char*) * (dst_argv.tbc + 1);
    const size_t envv_size = sizeof(char*) * (dst_env.tbc + 1);
    const size_t aux_size = sizeof(proc_aux_entry_t) * AUX_ENTRY_COUNT;
    const size_t pure_ctx_rgn_size = sizeof(proc_ctx_t) + argv_size + envv_size + aux_size + dst_argv.tb_strings_size + dst_env.tb_strings_size;
    const ssize_t ctx_rgn_size = __Ceil_PowerOf2(pure_ctx_rgn_size, CPU_PAGE_SIZE);

    try(AddressSpace_Allocate(&pimg->as, ctx_rgn_size, (void**)&pctx));

    dst_argv.tbv = (char**)((char*)pctx + sizeof(proc_ctx_t));
    dst_env.tbv = (char**)((char*)dst_argv.tbv + argv_size);
    aux_array = (proc_aux_entry_t*)((char*)dst_env.tbv + envv_size);
    dst_argv.tb_strings = (char*)aux_array + aux_size;
    dst_env.tb_strings = dst_argv.tb_strings + dst_argv.tb_strings_size;

    _copyin_table(argv, &dst_argv);
    _copyin_table(env, &dst_env);
    _copyin_aux_array(aux_array, pimg->base);

    pctx->argc = dst_argv.tbc;
    pctx->argv = dst_argv.tbv;
    pctx->envc = dst_env.tbc;
    pctx->envv = dst_env.tbv;
    pctx->aux = aux_array;

    pimg->ctx_base = pctx;
    pimg->arg_size = dst_argv.tb_strings_size;
    pimg->arg_strings = dst_argv.tb_strings;
    pimg->env_size = dst_env.tb_strings_size;
    pimg->env_strings = dst_env.tb_strings;

catch:
    return err;
}

// Initializes a new process image by loading the executable file and building
// up the initial memory map.
errno_t proc_img_create(FileManagerRef _Nonnull fm, const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable env[], proc_img_t* _Nullable * _Nonnull pOutImg)
{
    decl_try_err();
    proc_img_t* pimg;
    size_t argc = 0; 

    while (argv[argc]) {
        argc++;
    }
    const size_t img_size = sizeof(struct proc_img) + sizeof(char*) * (argc + 1);

    try(kalloc_cleared(img_size, (void**)&pimg));

    pimg->orig_path = path;
    pimg->orig_argv = argv;
    pimg->orig_env = env;
    pimg->fm = fm;

    AddressSpace_Init(&pimg->as);
    
    argc = 0;
    while(argv[argc]) {
        pimg->argv_buf[2 + argc] = argv[argc];
        argc++;
    }
    pimg->orig_argv = &pimg->argv_buf[2];

catch:
    *pOutImg = pimg;

    return err;
}

void proc_img_destroy(proc_img_t* _Nullable pimg)
{
    if (pimg) {
        _proc_img_close_file(pimg);
        AddressSpace_Deinit(&pimg->as);
        kfree(pimg);
    }
}

errno_t proc_img_replace_arg0(proc_img_t* _Nonnull pimg, const char* _Nonnull interp, const char* _Nullable interp_arg, const char* _Nonnull script_path)
{
    if (pimg->orig_argv == &pimg->argv_buf[0]) {
        // already replaced arg0
        return EINVAL;
    }

    if (interp_arg) {
        pimg->argv_buf[0] = interp;
        pimg->argv_buf[1] = interp_arg;
        pimg->argv_buf[2] = script_path;
        pimg->orig_argv = &pimg->argv_buf[0];
    }
    else {
        pimg->argv_buf[1] = interp;
        pimg->argv_buf[2] = script_path;
        pimg->orig_argv = &pimg->argv_buf[1];
    }
}

errno_t proc_img_open_file(proc_img_t* _Nonnull pimg, const char* _Nonnull path)
{
    decl_try_err();

    _proc_img_close_file(pimg);
    pimg->orig_path = path;


    try(FileManager_OpenFile(pimg->fm, pimg->orig_path, O_RDONLY | _O_EXONLY, &pimg->file));
    try(IOChannel_GetAttributes(pimg->file, &pimg->file_attr));

    // Do some basic file validation
    if (pimg->file_attr.file_type != FS_FTYPE_REG) {
        throw(EACCESS);
    }
    if (pimg->file_attr.size < 2) {
        throw(ENOEXEC);
    }
    else if (pimg->file_attr.size > SIZE_MAX) {
        throw(ENOMEM);
    }


    // Read the file prefix and leave the seek position at the end of the prefix.
    err = IOChannel_Read(pimg->file, pimg->prefix_buf, PROC_IMG_PREFIX_SIZE, &pimg->prefix_length);

catch:
    if (err != EOK) {
        _proc_img_close_file(pimg);
    }

    return err;
}

static void _proc_img_close_file(proc_img_t* _Nonnull pimg)
{
    if (pimg->file) {
        IOChannel_Release(pimg->file);
        pimg->file = NULL;
    }
}

errno_t proc_img_load(proc_img_t* _Nonnull pimg)
{
    decl_try_err();
    int ldr_idx = 0;

    // Open the original executable file
    try(proc_img_open_file(pimg, pimg->orig_path));


    // Find a loader that can handle the file type and let it do its magic
    while (ldr_idx < LDR_COUNT) {
        ldr_func_t loader = ldr_table[ldr_idx];

        err = loader(pimg);
        if (err == EOK || err != ENOEXEC || (err == ENOEXEC && ldr_idx == LDR_COUNT-1)) {
            break;
        }

        // try next loader
        ldr_idx++;
    }


    // Create the proc context
    if (err == EOK) {
        err = _build_ctx(pimg, pimg->orig_argv, pimg->orig_env);
    }

catch:
    _proc_img_close_file(pimg);

    return err;
}

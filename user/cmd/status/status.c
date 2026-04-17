//
//  status.c
//  cmds
//
//  Created by Dietmar Planitzer on 5/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <errno.h>
#include <ext/stdlib.h>
#include <stdio.h>
#include <string.h>
#include "run_proc.h"
#include "table.h"
#include "utils.h"

static char num_buf[__LLONG_MAX_BASE_10_DIGITS + 1];


static const table_column_t g_table_cols[] = {
    {
        .title = "PID",
        .width = 6,
        .align = TABLE_ALIGN_LEFT
    },
    {
        .title = "Command",
        .width = 12,
        .align = TABLE_ALIGN_LEFT
    },
    {
        .title = "#VCPU",
        .width = 6,
        .align = TABLE_ALIGN_LEFT
    },
    {
        .title = "VM Size",
        .width = 8,
        .align = TABLE_ALIGN_LEFT
    },
    {
        .title = "State",
        .width = 8,
        .align = TABLE_ALIGN_LEFT
    },
    {
        .title = "UID",
        .width = 8,
        .align = TABLE_ALIGN_LEFT
    },
    {
        .title = "SID",
        .width = 4,
        .align = TABLE_ALIGN_LEFT
    },
    {
        .title = "PGRP",
        .width = 4,
        .align = TABLE_ALIGN_LEFT
    },
    {
        .title = "PPID",
        .width = 4,
        .align = TABLE_ALIGN_LEFT
    }
};


CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("status")
);


#define COL_PID     0
#define COL_NAME    1
#define COL_NVCPU   2
#define COL_VMSIZ   3
#define COL_STATE   4
#define COL_UID     5
#define COL_SID     6
#define COL_PGRP    7
#define COL_PPID    8

static const char* _Nonnull run_proc_cell_func(void* ctx, int row, int col)
{
    run_proc_t* rp = run_proc_for_index(row);

    switch (col) {
        case COL_PID:
            return itoa(rp->pid, num_buf, 10);

        case COL_NAME:
            return rp->name;

        case COL_NVCPU:
            return itoa(rp->vcpu_count, num_buf, 10);

        case COL_VMSIZ:
            return (rp->pid > 1) ? fmt_mem_size(rp->vm_size, num_buf) : "-";

        case COL_STATE:
            return run_proc_state_name(rp->state);

        case COL_UID:
            return itoa(rp->uid, num_buf, 10);

        case COL_SID:
            return itoa(rp->sid, num_buf, 10);

        case COL_PGRP:
            return itoa(rp->pgrp, num_buf, 10);

        case COL_PPID:
            return itoa(rp->ppid, num_buf, 10);

        default:
            return "";
    }
}

static int show_procs(void)
{
    run_procs_sample();

    table_t* tp = table_create(g_table_cols, sizeof(g_table_cols) / sizeof(table_column_t));
    table_set_cell_func(tp, (table_cell_func_t)run_proc_cell_func, NULL);
    table_set_viewport(tp, 0, 20);
//    table_set_fill_viewport(tp, true);

    table_set_row_count(tp, run_proc_count());

    //term_cls();

    const run_procs_info_t* rp_info = run_procs_info();
    printf("Processes: %zu total, %zu running, %zu sleeping\n", rp_info->proc_count, rp_info->run_proc_count, rp_info->slp_proc_count);
    printf("VCPUs: %zu acquired    CPUs: %zu\n", rp_info->vcpu_count, rp_info->cpu_count);
    printf("RAM: %s total\n\n", fmt_mem_size(rp_info->phys_mem_size, num_buf));
//    printf("RAM: %llu\n\n", rp_info->phys_mem_size);
    
    table_draw(tp);

    return (errno == 0) ? 0 : -1;
}


int main(int argc, char* argv[])
{
    clap_parse(0, params, argc, argv);
    
    if (show_procs() == 0) {
        return EXIT_SUCCESS;
    }
    else {
        clap_error(argv[0], "%s", strerror(errno));
        return EXIT_FAILURE;
    }
}

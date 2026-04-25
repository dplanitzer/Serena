//
//  status.c
//  cmds
//
//  Created by Dietmar Planitzer on 5/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <errno.h>
#include <limits.h>
#include <ext/stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ext/math.h>
#include <ext/nanotime.h>
#include <serena/clock.h>
#include <serena/fd.h>
#include "run_proc.h"
#include "table.h"
#include "utils.h"

#define COL_PID     0
#define COL_COMMAND 1
#define COL_TIME    2
#define COL_NVCPU   3
#define COL_MEM     4
#define COL_STATE   5
#define COL_UID     6
#define COL_PGRP    7
#define COL_PPID    8


static char num_buf[FMT_MEM_SIZE_BUFFER_SIZE];
static char dur_buf[FMT_DURATION_BUFFER_SIZE];
static run_proc_t** g_rps;
static size_t       g_rps_capacity;
static int          g_sort_by = COL_PID;
static table_t*     g_table;


static const table_column_t g_table_cols[] = {
    {
        .title = "PID",
        .width = 6,
        .align = TABLE_ALIGN_LEFT
    },
    {
        .title = "COMMAND",
        .width = 12,
        .align = TABLE_ALIGN_LEFT
    },
    {
        .title = "TIME",
        .width = 8,
        .align = TABLE_ALIGN_LEFT
    },
    {
        .title = "#VCPU",
        .width = 6,
        .align = TABLE_ALIGN_LEFT
    },
    {
        .title = "MEM",
        .width = 8,
        .align = TABLE_ALIGN_LEFT
    },
    {
        .title = "STATE",
        .width = 8,
        .align = TABLE_ALIGN_LEFT
    },
    {
        .title = "UID",
        .width = 8,
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


static int sort_rp(run_proc_t** prp0, run_proc_t** prp1)
{
    const run_proc_t* rp0 = *prp0;
    const run_proc_t* rp1 = *prp1;

    switch (g_sort_by) {
        case COL_PID:
            return rp1->pid - rp0->pid;

        case COL_COMMAND:
            return strcmp(rp1->name, rp0->name);

        case COL_TIME:
            return rp1->run_time.tv_sec - rp0->run_time.tv_sec;

        case COL_NVCPU:
            return rp1->vcpu_count - rp0->vcpu_count;

        case COL_MEM:
            return rp1->vm_size - rp0->vm_size;

        case COL_STATE:
            return rp1->state - rp0->state;

        case COL_UID:
            return rp1->uid - rp0->uid;

        case COL_PGRP:
            return rp1->pgrp - rp0->pgrp;

        case COL_PPID:
            return rp1->ppid - rp0->ppid;

        default:
            return -1;
    }
}

static const char* _Nonnull display_rp(void* ctx, int row, int col)
{
    run_proc_t* rp = g_rps[row];

    switch (col) {
        case COL_PID:
            return itoa(rp->pid, num_buf, 10);

        case COL_COMMAND:
            return rp->name;

        case COL_TIME:
            return fmt_duration(&rp->run_time, dur_buf);

        case COL_NVCPU:
            return itoa(rp->vcpu_count, num_buf, 10);

        case COL_MEM:
            return (rp->pid > 1) ? fmt_mem_size(rp->vm_size, num_buf) : "-";

        case COL_STATE:
            return run_proc_state_name(rp->state);

        case COL_UID:
            return itoa(rp->uid, num_buf, 10);

        case COL_PGRP:
            return itoa(rp->pgrp, num_buf, 10);

        case COL_PPID:
            return itoa(rp->ppid, num_buf, 10);

        default:
            return "";
    }
}

static run_proc_t** get_rps_array_of_size(size_t targetSize)
{
    if (g_rps_capacity < targetSize) {
        const size_t new_rps_capacity = __max(targetSize, g_rps_capacity*2);
        run_proc_t** new_rps = realloc(g_rps, new_rps_capacity * sizeof(run_proc_t*));

        if (new_rps == NULL) {
            return NULL;
        }

        g_rps = new_rps;
        g_rps_capacity = new_rps_capacity;
    }

    return g_rps;
}

static void display_status(void)
{
    run_procs_sample();

    const size_t nprocs = run_procs_count();
    run_proc_t** rps = get_rps_array_of_size(nprocs);
    if (rps == NULL) {
        return;
    }


    run_procs_get_all(rps, nprocs);
    qsort(rps, nprocs, sizeof(run_proc_t*), (int (*)(const void*, const void*))sort_rp);


    table_set_row_count(g_table, nprocs);
    term_cls();

    const run_procs_info_t* rp_info = run_procs_info();
    printf("Processes: %zu total, %zu running, %zu sleeping\n", rp_info->proc_count, rp_info->run_proc_count, rp_info->slp_proc_count);
    printf("VCPUs: %zu acquired    CPUs: %zu\n", rp_info->vcpu_count, rp_info->cpu_count);
    printf("RAM: %s total\n\n", fmt_mem_size(rp_info->phys_mem_size, num_buf));
    
    table_draw(g_table);
}

static bool init(void)
{
    g_table = table_create(g_table_cols, sizeof(g_table_cols) / sizeof(table_column_t));
    if (g_table == NULL) {
        return false;
    }

    table_set_cell_func(g_table, (table_cell_func_t)display_rp, NULL);
    table_set_viewport(g_table, 0, 18); //XXX hard coded display height for NTSC. Should dynamically adjust with true terminal height 
    table_set_fill_viewport(g_table, true);

    term_cursor_on(false);
    fd_setflags(FD_STDIN, FD_FOP_ADD, O_NONBLOCK);

    return true;
}

static void cleanup(void)
{
    term_cls();
    term_cursor_on(true);
    fd_setflags(FD_STDIN, FD_FOP_REMOVE, O_NONBLOCK);
}

static void main_loop(void)
{
    nanotime_t wt;
    nanotime_from_ms(&wt, 250);
    unsigned int counter = 0;
    char ch;

    for (;;) {
        clock_wait(CLOCK_MONOTONIC, 0, &wt, NULL);

        ch = '\0';
        fd_read(FD_STDIN, &ch, 1);

        if (ch == 'q' || ch == 'Q' || ch == 'x' || ch == 'X') {
            break;
        }


        if (counter % 4 == 0) {
            display_status();
        }

        counter++;
    }
}

int main(int argc, char* argv[])
{
    clap_parse(0, params, argc, argv);

    if (init()) {
        main_loop();
    }
    cleanup();

    return EXIT_SUCCESS;
}

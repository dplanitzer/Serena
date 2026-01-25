//
//  hellodispatch.c
//  hellodispatch
//
//  Created by Dietmar Planitzer on 1/24/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <dispatch.h>
#include <stdio.h>
#include <stdlib.h>


static const char* gText = "Hello Dispatch!\n";
static int gIndex;

static void print_loop(void* _Nullable ignore)
{
    putc(gText[gIndex++], stdout);
    fflush(stdout);

    if (gText[gIndex] == '\0') {
        dispatch_cancel_item(dispatch_main_queue(), dispatch_current_item());
        exit(0);
    }
}


int main(int argc, char *argv[])
{
    struct timespec per_char_delay;

    timespec_from_ms(&per_char_delay, 200);
    gIndex = 0;

    dispatch_repeating(dispatch_main_queue(), 0, &TIMESPEC_ZERO, &per_char_delay, (dispatch_async_func_t)print_loop, NULL);
    dispatch_run_main_queue();
    
    /* NOT REACHED */
    return 0;
}

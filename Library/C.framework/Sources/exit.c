//
//  exit.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <__globals.h>
#include <__stddef.h>


typedef void (*AtExitFunc)(void);

typedef struct _AtExitEntry {
    SListNode           node;
    AtExitFunc _Nonnull func;
} AtExitEntry;


void __exit_init(void)
{
    // XXX protect with a lock
    SList_Init(&__gAtExitQueue);
}

// Not very efficient and that's fine. The expectation is that this will be used
// very rarely. So we rather keep the memory consumption very low for the majority
// that doesn't use this.
int atexit(void (*func)(void))
{
    AtExitEntry* p = (AtExitEntry*)malloc(sizeof(AtExitEntry));

    if (p == NULL) {
        return ENOMEM;
    }

    SListNode_Init(&p->node);
    p->func = func;
    SList_InsertBeforeFirst(&__gAtExitQueue, &p->node);

    return 0;
}

_Noreturn exit(int exit_code)
{
    SList_ForEach(&__gAtExitQueue, AtExitEntry, {
        pCurNode->func();
    });

    _Exit(exit_code);
}

_Noreturn _Exit(int exit_code)
{
    __syscall(SC_exit, exit_code);
}

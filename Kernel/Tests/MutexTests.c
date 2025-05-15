//
//  MutexTests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 8/24/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/dispatch.h>
#include <sys/mutex.h>
#include <System/TimeInterval.h>
#include <System/_math.h>
#include "Asserts.h"


#define NUM_WORKERS     16
#define NUM_VPS         4
#define NUM_PATTERNS    8

static const char* gAvailablePattern[NUM_PATTERNS] = {
    "Hello World Out There Or So",
    "The quick brown fox jumped over something",
    "Tomorrow isn't Today and neither Yesterday",
    "The purpose of a Kernel is to do stuff",
    "which is different from userspace, because",
    "the apps over there do stuff in a different way",
    "Rockets are faster than cars I think, though not quite sure",
    "About that and whether ships aren't the fastest of them all!",
};

static int gQueue;
static mutex_t gMutex;
static int gCurrentPatternIndex;
static char gCurrentPattern[256];


static void select_and_write_pattern(void)
{
    gCurrentPatternIndex = (gCurrentPatternIndex + 1) % NUM_PATTERNS;

    char* dst = gCurrentPattern;
    const char* src = gAvailablePattern[gCurrentPatternIndex];
    size_t len = strlen(src) + 1;
    TimeInterval dl = TimeInterval_MakeMilliseconds(4);

    while (len > 0) {
        const size_t nBytesToCopy = __min(len, 4);

        memcpy(dst, src, nBytesToCopy);
        src += nBytesToCopy;
        dst += nBytesToCopy;
        len -= nBytesToCopy;

       //assertOK(clock_wait(CLOCK_UPTIME, &dl));
    }

    printf("W: '%s'\n", gCurrentPattern);
}

static void OnWork(void* _Nonnull pValue)
{
    assertOK(mutex_lock(&gMutex));
    printf("R: %d\n", gCurrentPatternIndex);

    // Make sure that we find the expected pattern in the pattern buffer
    assertTrue(gCurrentPatternIndex >= 0 && gCurrentPatternIndex < NUM_PATTERNS);
    assertEquals(0, strcmp(gCurrentPattern, gAvailablePattern[gCurrentPatternIndex]));

    // Select a new pattern and write it to the pattern buffer
    select_and_write_pattern();

    assertOK(mutex_unlock(&gMutex));

    assertOK(dispatch_async(gQueue, OnWork, NULL));
}


void mutex_test(int argc, char *argv[])
{
    assertOK(mutex_init(&gMutex));
    assertOK(dispatch_create(0, NUM_VPS, kDispatchQoS_Utility, kDispatchPriority_Normal, &gQueue));

    gCurrentPatternIndex = 0;
    select_and_write_pattern();

    for (size_t i = 0; i < NUM_WORKERS; i++) {
        assertOK(dispatch_async(gQueue, OnWork, NULL));
    }
}

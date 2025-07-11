//
//  semaphore_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 7/8/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/os_dispatch.h>
#include <sys/semaphore.h>
#include <sys/timespec.h>
#include <_math.h>
#include "Asserts.h"

// See: https://medium.com/swlh/the-dining-philosophers-problem-solution-in-c-90e2593f64e8
#define NUM 5
#define LEFT(p) ((p) % NUM)
#define RIGHT(p) (((p) + 1) % NUM)

static sem_t room;
static sem_t chopstick[NUM];
static int phil[NUM] = { 0, 1, 2, 3, 4 };
static int gQueue;


static void philosopher(int* num)
{
    const int p = *num;

    while (true) {
        assertOK(sem_wait(&room, 1));
        printf("Philosopher %d has entered room\n", p);
        
        assertOK(sem_wait(&chopstick[LEFT(p)], 1));
        assertOK(sem_wait(&chopstick[RIGHT(p)], 1));

        printf("Philosopher %d is eating...\n", p);
        sleep(2);
        printf("Philosopher %d has finished eating\n", p);

        assertOK(sem_post(&chopstick[RIGHT(p)], 1));
        assertOK(sem_post(&chopstick[LEFT(p)], 1));
        
        assertOK(sem_post(&room, 1));
    }
}

void sema_test(int argc, char *argv[])
{
    gQueue = os_dispatch_create(NUM, NUM, kDispatchQoS_Utility, kDispatchPriority_Normal);

    assertGreaterEqual(0, gQueue);
    assertOK(sem_init(&room, NUM-1));
    
    for (int i = 0; i < NUM; i++) {
        assertOK(sem_init(&chopstick[i], 1));
    }

    for (int i = 0; i < NUM; i++) {
        assertOK(os_dispatch_async(gQueue, (os_dispatch_func_t)philosopher, &phil[i]));
    }
}

#include <lib/x86.h>
#include <lib/debug.h>
#include <thread/PTCBIntro/export.h>
#include <thread/PTQueueInit/export.h>
#include "export.h"
#include "import.h"
extern struct TCB TCBPool[NUM_IDS];

int PThread_test_own()
{
    unsigned int tid;

    /* Test 1: priority-based selection */
    ready_enqueue(5, 3);
    ready_enqueue(6, 7);
    ready_enqueue(7, 7);

    tid = ready_dequeue();
    if (tid != 6) {
        dprintf("own test 1 failed: expected pid=6, got %d\n", tid);
        return 1;
    }
    /* Test 2: RR within same priority */
    tid = ready_dequeue();
    if (tid != 7) {
        dprintf("own test 2 failed: expected pid=7, got %d\n", tid);
        return 1;
    }
    /* Test 3: remaining process dequeued */
    tid = ready_dequeue();
    if (tid != 5) {
        dprintf("own test 3 failed: expected pid=5, got %d\n", tid);
        return 1;
    }
    /* Test 4: empty queue returns NUM_IDS */
    tid = ready_dequeue();
    if (tid != NUM_IDS) {
        dprintf("own test 4 failed: expected empty queue\n");
        return 1;
    }

    /* Test 5: default priority should be 5 */
    if (tcb_get_priority(20) != 5) {
        dprintf("own test 5 failed: default priority should be 5\n");
        return 1;
    }

    /* Test 6 : priority ordering */
    ready_enqueue(8, 9);
    ready_enqueue(9, 0);
    tid = ready_dequeue();
    if (tid != 8) {
        dprintf("own test 6 failed: highest priority not selected\n");
        return 1;
    }
    ready_dequeue();

    /* Test 7: aging exact threshold */
    TCBPool[5].waiting_time = 0;
    ready_enqueue(5, 3);
    unsigned int i;
    for (i = 0; i < AGING_THRESHOLD - 1; i++) {
        TCBPool[5].waiting_time++;
    }
    if (tcb_get_priority(5) != 3) {
        dprintf("own test 7 failed: promoted too early\n");
        ready_dequeue();
        return 1;
    }
    TCBPool[5].waiting_time++;
    if (TCBPool[5].waiting_time >= AGING_THRESHOLD) {
        TCBPool[5].waiting_time = 0;
        ready_remove(5);
        ready_enqueue(5, tcb_get_priority(5) + 1);
    }
    if (tcb_get_priority(5) != 4) {
        dprintf("own test 7 failed: not promoted at threshold\n");
        ready_dequeue();
        return 1;
    }
    ready_dequeue();


    /* Test 10: cpu score heavy - full quantum, priority should decrease */
    TCBPool[7].cpu_score = 50;
    TCBPool[7].cpu_ticks = SCHED_SLICE;
    tcb_set_priority(7, 5);
    int recent_cpu = (TCBPool[7].cpu_ticks * 100) / SCHED_SLICE;
    if (recent_cpu > 100) recent_cpu = 100;
    int new_score = (ALPHA * recent_cpu + (100 - ALPHA) * TCBPool[7].cpu_score) / 100;
    if (new_score > 100) new_score = 100;
    TCBPool[7].cpu_score = new_score;
    int prio = tcb_get_priority(7);
    if (new_score >= CPU_HIGH) {
        if (prio > MIN_PRIORITY) prio--;
    }
    tcb_set_priority(7, prio);
    if (tcb_get_priority(7) != 4) {
        dprintf("own test 9 failed: expected priority 4, got %d\n", tcb_get_priority(7));
        return 1;
    }
    if (TCBPool[7].cpu_score != 75) {
        dprintf("own test 9 failed: expected score 75, got %d\n", TCBPool[7].cpu_score);
        return 1;
    }

    /* Test 11: cpu score io bound - blocks early, priority unchanged */
    TCBPool[8].cpu_score = 50;
    TCBPool[8].cpu_ticks = 2;
    tcb_set_priority(8, 5);
    recent_cpu = (TCBPool[8].cpu_ticks * 100) / SCHED_SLICE;
    if (recent_cpu > 100) recent_cpu = 100;
    new_score = (ALPHA * recent_cpu + (100 - ALPHA) * TCBPool[8].cpu_score) / 100;
    if (new_score < 0) new_score = 0;
    TCBPool[8].cpu_score = new_score;
    prio = tcb_get_priority(8);
    if (new_score >= CPU_HIGH) {
        if (prio > MIN_PRIORITY) prio--;
    } else if (new_score <= CPU_LOW) {
        if (prio < MAX_PRIORITY) prio++;
    }
    tcb_set_priority(8, prio);
    if (tcb_get_priority(8) != 5) {
        dprintf("own test 10 failed: expected priority 5, got %d\n", tcb_get_priority(8));
        return 1;
    }

    dprintf("own test passed.\n");
    return 0;
}

int test_PThread()
{
    int failed = PThread_test_own();
    return (failed == 0) ? 1 : 0;
}
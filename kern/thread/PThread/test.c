#include <lib/x86.h>
#include <lib/debug.h>
#include <thread/PTCBIntro/export.h>
#include <thread/PTQueueInit/export.h>
#include "export.h"



int PThread_test_own()
{
    unsigned int tid;

    /* Test 1: ready_enqueue and ready_dequeue basic */
    ready_enqueue(5, 3);   // pid=5, priority=3
    ready_enqueue(6, 7);   // pid=6, priority=7
    ready_enqueue(7, 7);   // pid=7, priority=7

    // Should dequeue highest priority first (7)
    tid = ready_dequeue();
    if (tid != 6) {
        dprintf("own test 1 failed: expected pid=6, got %d\n", tid);
        return 1;
    }

    // RR within same priority: next should be pid=7
    tid = ready_dequeue();
    if (tid != 7) {
        dprintf("own test 2 failed: expected pid=7, got %d\n", tid);
        return 1;
    }

    // Now only pid=5 at priority 3 remains
    tid = ready_dequeue();
    if (tid != 5) {
        dprintf("own test 3 failed: expected pid=5, got %d\n", tid);
        return 1;
    }

    // Queue should be empty now
    tid = ready_dequeue();
    if (tid != NUM_IDS) {
        dprintf("own test 4 failed: expected empty queue\n");
        return 1;
    }

    /* Test 2: default priority should be 5 */
    if (tcb_get_priority(20) != 5) {
        dprintf("own test 5 failed: default priority should be 5\n");
        return 1;
    }

    /* Test 3: priority ordering */
    ready_enqueue(8, 9);
    ready_enqueue(9, 0);
    tid = ready_dequeue();
    if (tid != 8) {
        dprintf("own test 6 failed: highest priority not selected\n");
        return 1;
    }
    ready_dequeue(); // cleanup

    dprintf("own test passed.\n");
    return 0;
}

int test_PThread()
{
    int failed = PThread_test_own();
    return (failed==0)?1:0;
}
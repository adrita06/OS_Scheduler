#include <lib/x86.h>
#include <lib/debug.h>
#include <pcpu/PCPUIntro/export.h>
#include <thread/PTCBIntro/export.h>
#include <thread/PTQueueIntro/export.h>
#include "export.h"
#include "import.h"

static void ptqueueinit_reset_fixture(void)
{
    unsigned int cpu, chid, pid;

    set_pcpu_idx(0, 0);

    for (cpu = 0; cpu < NUM_CPUS; cpu++) {
        for (chid = 0; chid < NUM_IDS + NPRIO; chid++) {
            tqueue_init_at_id(cpu, chid);
        }
    }

    for (pid = 0; pid < NUM_IDS; pid++) {
        tcb_init_at_id(0, pid);
    }
}

static void ptqueueinit_use_cpu(unsigned int cpu_idx)
{
    set_pcpu_idx(0, cpu_idx);
}

int PTQueueInit_test1()
{                                   
    unsigned int cpu, i;

    ptqueueinit_reset_fixture();

    for (cpu = 0; cpu < NUM_CPUS; cpu++) {
        for (i = 0; i < NUM_IDS + NPRIO; i++) {
            if (tqueue_get_head_at(cpu, i) != NUM_IDS ||
                tqueue_get_tail_at(cpu, i) != NUM_IDS) {
                dprintf("test 1 failed.\n");
                return 1;
            }
        }
    }
    dprintf("ptqueueinit test 1 passed.\n");
    return 0;
}

int PTQueueInit_test2()
{
    unsigned int pid;

    ptqueueinit_reset_fixture();

    tqueue_enqueue(0, 2);
    tqueue_enqueue(0, 3);
    tqueue_enqueue(0, 4);
    if (tcb_get_prev(2) != NUM_IDS || tcb_get_next(2) != 3) {
        dprintf("test 2 failed.\n");
        return 1;
    }
    if (tcb_get_prev(3) != 2 || tcb_get_next(3) != 4) {
        dprintf("test 2 failed.\n");
        return 1;
    }
    if (tcb_get_prev(4) != 3 || tcb_get_next(4) != NUM_IDS) {
        dprintf("test 2 failed.\n");
        return 1;
    }
    tqueue_remove(0, 3);
    if (tcb_get_prev(2) != NUM_IDS || tcb_get_next(2) != 4) {
        dprintf("test 2 failed.\n");
        return 1;
    }
    if (tcb_get_prev(3) != NUM_IDS || tcb_get_next(3) != NUM_IDS) {
        dprintf("test 2 failed.\n");
        return 1;
    }
    if (tcb_get_prev(4) != 2 || tcb_get_next(4) != NUM_IDS) {
        dprintf("test 2 failed.\n");
        return 1;
    }
    pid = tqueue_dequeue(0);
    if (pid != 2 || tcb_get_prev(pid) != NUM_IDS || tcb_get_next(pid) != NUM_IDS
        || tqueue_get_head(0) != 4 || tqueue_get_tail(0) != 4) {
        dprintf("test 2 failed.\n");
        return 1;
    }
    dprintf("test 2 passed.\n");
    return 0;
}

int PTQueueInit_test_own()
{
    unsigned int pid;

    ptqueueinit_reset_fixture();

    // Enqueue at different priorities and verify dequeue order (highest first)
    ready_enqueue(5, 3);   // pid 5 at priority 3
    ready_enqueue(6, 7);   // pid 6 at priority 7
    ready_enqueue(7, 1);   // pid 7 at priority 1

    pid = ready_dequeue();
    if (pid != 6) {        // priority 7 should come out first
        dprintf("own test failed: expected pid 6, got %d\n", pid);
        return 1;
    }
    pid = ready_dequeue();
    if (pid != 5) {        // priority 3 next
        dprintf("own test failed: expected pid 5, got %d\n", pid);
        return 1;
    }
    pid = ready_dequeue();
    if (pid != 7) {        // priority 1 last
        dprintf("own test failed: expected pid 7, got %d\n", pid);
        return 1;
    }
    ready_enqueue(11,99);
    if(tcb_get_priority(11)!= MAX_PRIORITY){
        dprintf("own test failed: priority clamping to MAX failed\n");
        return 1;
    }
    ready_dequeue();
    ready_enqueue(12,-1);
    if(tcb_get_priority(12)!= MIN_PRIORITY){
        dprintf("own test failed: priority clamping to MIN failed\n");
        return 1;
    }
    ready_dequeue();



    dprintf("own test passed.\n");
    return 0;
}

int PTQueueInit_test_load_balance_steal(void)
{
    unsigned int pid;

    ptqueueinit_reset_fixture();

    ptqueueinit_use_cpu(0);
    ready_enqueue(20, 4);
    ready_enqueue(21, 9);
    ready_enqueue(22, 1);

    ptqueueinit_use_cpu(2);
    ready_enqueue(30, 8);

    ptqueueinit_use_cpu(1);
    pid = ready_dequeue();
    if (pid != 21) {
        dprintf("load balance test 1 failed: expected stolen pid 21, got %d\n", pid);
        return 1;
    }
    if (tcb_get_ready_cpu(21) != -1) {
        dprintf("load balance test 1 failed: stolen pid still marked ready on a cpu\n");
        return 1;
    }
    if (tqueue_get_head_at(0, READY_QUEUE(9)) != NUM_IDS) {
        dprintf("load balance test 1 failed: highest-priority donor entry not removed\n");
        return 1;
    }
    if (tqueue_get_head_at(0, READY_QUEUE(4)) != 20 ||
        tqueue_get_head_at(0, READY_QUEUE(1)) != 22) {
        dprintf("load balance test 1 failed: donor queue state corrupted after steal\n");
        return 1;
    }

    dprintf("load balance test 1 passed.\n");
    return 0;
}

int PTQueueInit_test_load_balance_prefers_local(void)
{
    unsigned int pid;

    ptqueueinit_reset_fixture();

    ptqueueinit_use_cpu(0);
    ready_enqueue(40, 8);
    ready_enqueue(41, 7);

    ptqueueinit_use_cpu(1);
    ready_enqueue(42, 3);
    pid = ready_dequeue();
    if (pid != 42) {
        dprintf("load balance test 2 failed: expected local pid 42, got %d\n", pid);
        return 1;
    }
    if (tqueue_get_head_at(0, READY_QUEUE(8)) != 40 ||
        tqueue_get_head_at(0, READY_QUEUE(7)) != 41) {
        dprintf("load balance test 2 failed: local dequeue stole work unnecessarily\n");
        return 1;
    }

    dprintf("load balance test 2 passed.\n");
    return 0;
}

int PTQueueInit_test_ready_remove_cross_cpu(void)
{
    ptqueueinit_reset_fixture();

    ptqueueinit_use_cpu(2);
    ready_enqueue(50, 6);
    if (tcb_get_ready_cpu(50) != 2) {
        dprintf("load balance test 3 failed: expected pid 50 to belong to cpu 2\n");
        return 1;
    }

    ptqueueinit_use_cpu(0);
    ready_remove(50);
    if (tcb_get_ready_cpu(50) != -1) {
        dprintf("load balance test 3 failed: ready_remove did not clear owner cpu\n");
        return 1;
    }
    if (tqueue_get_head_at(2, READY_QUEUE(6)) != NUM_IDS ||
        tqueue_get_tail_at(2, READY_QUEUE(6)) != NUM_IDS) {
        dprintf("load balance test 3 failed: remote ready queue still contains pid 50\n");
        return 1;
    }
    if (tcb_get_prev(50) != NUM_IDS || tcb_get_next(50) != NUM_IDS) {
        dprintf("load balance test 3 failed: pid 50 links not cleared\n");
        return 1;
    }

    dprintf("load balance test 3 passed.\n");
    return 0;
}

int test_PTQueueInit()
{
    int failed = PTQueueInit_test1()
        + PTQueueInit_test2()
        + PTQueueInit_test_own()
        + PTQueueInit_test_load_balance_steal()
        + PTQueueInit_test_load_balance_prefers_local()
        + PTQueueInit_test_ready_remove_cross_cpu();

    return (failed == 0)?1:0;
}

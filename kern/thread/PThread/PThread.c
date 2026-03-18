#include <lib/x86.h>
#include <lib/thread.h>
#include <lib/spinlock.h>
#include <lib/debug.h>
#include <dev/lapic.h>
#include <pcpu/PCPUIntro/export.h>
#include <kern/thread/PTCBIntro/export.h>

#define ALPHA 50
#define CPU_HIGH 75
#define CPU_LOW 25
#define MAX_PRIORITY 9
#define MIN_PRIORITY 0
#define AGING_THRESHOLD  10   // boost after waiting 10 time slices
#define READY_QUEUE(p) (NUM_IDS + (p))
#define IDLE_TID_BASE (NUM_IDS - NUM_CPUS)

#include "import.h"

extern struct TCB TCBPool[NUM_IDS];

static spinlock_t sched_lk;

unsigned int sched_ticks[NUM_CPUS];

static unsigned int idle_tid_for_cpu(unsigned int cpu_idx)
{
    return IDLE_TID_BASE + cpu_idx;
}

static int is_idle_tid(unsigned int pid)
{
    return pid >= IDLE_TID_BASE && pid < NUM_IDS;
}

void thread_init(unsigned int mbi_addr)
{
    unsigned int i;
    for (i = 0; i < NUM_CPUS; i++) {
        sched_ticks[i] = 0;
    }

    spinlock_init(&sched_lk);
    tqueue_init(mbi_addr);
    set_curid(0);
    tcb_set_state(0, TSTATE_RUN);
}

unsigned int thread_spawn(void *entry, unsigned int id, unsigned int quota)
{
    unsigned int pid;

    spinlock_acquire(&sched_lk);

    pid = kctx_new(entry, id, quota);
    if (pid == NUM_IDS) {
        spinlock_release(&sched_lk);
        return NUM_IDS;
    }

    tcb_set_state(pid, TSTATE_READY);
    ready_enqueue(pid, tcb_get_priority(pid));  // CHANGED

    spinlock_release(&sched_lk);

    return pid;
}

void thread_yield(void)
{
    unsigned int old_cur_pid;
    unsigned int new_cur_pid;

    spinlock_acquire(&sched_lk);

    old_cur_pid = get_curid();
    tcb_set_state(old_cur_pid, TSTATE_READY);
    ready_enqueue(old_cur_pid, tcb_get_priority(old_cur_pid)); 

    new_cur_pid = ready_dequeue();                               
    if (new_cur_pid == NUM_IDS)
        new_cur_pid = old_cur_pid;

    TCBPool[new_cur_pid].waiting_time = 0;
    tcb_set_state(new_cur_pid, TSTATE_RUN);
    set_curid(new_cur_pid);

    if (old_cur_pid != new_cur_pid) {
        spinlock_release(&sched_lk);
        kctx_switch(old_cur_pid, new_cur_pid);
    } else {
        spinlock_release(&sched_lk);
    }
}

void sched_update(void)
{
    spinlock_acquire(&sched_lk);

    unsigned int cpu = get_pcpu_idx();
    unsigned int pid = get_curid();

    if (is_idle_tid(pid)) {
        spinlock_release(&sched_lk);
        return;
    }

    sched_ticks[cpu] += (1000 / LAPIC_TIMER_INTR_FREQ);
    TCBPool[pid].cpu_ticks++;

    if (sched_ticks[cpu] >= SCHED_SLICE) {

        sched_ticks[cpu] = 0;

        /* -------- CPU SCORE UPDATE (unchanged) -------- */
        int recent_cpu = (TCBPool[pid].cpu_ticks * 100) / SCHED_SLICE;
        if (recent_cpu > 100) recent_cpu = 100;

        int new_score = (ALPHA * recent_cpu +
                        (100 - ALPHA) * TCBPool[pid].cpu_score) / 100;
        if (new_score < 0)   new_score = 0;
        if (new_score > 100) new_score = 100;

        TCBPool[pid].cpu_score = new_score;
        TCBPool[pid].cpu_ticks = 0;

        int prio = tcb_get_priority(pid);
        if (new_score >= CPU_HIGH) {
            if (prio > MIN_PRIORITY) prio--;
        } else if (new_score <= CPU_LOW) {
            if (prio < MAX_PRIORITY) prio++;
        }
        tcb_set_priority(pid, prio);

        /* -------- AGING (NEW) -------- */
        int q;
        for (q = MIN_PRIORITY; q < MAX_PRIORITY; q++) {
            unsigned int qid  = READY_QUEUE(q);
            unsigned int tid  = tqueue_get_head(qid);

            while (tid != NUM_IDS) {
                unsigned int next_tid = tcb_get_next(tid); // save before remove
                TCBPool[tid].waiting_time++;

                if (TCBPool[tid].waiting_time >= AGING_THRESHOLD) {
                    TCBPool[tid].waiting_time = 0;
                    if (q + 1 <= MAX_PRIORITY){
                        tqueue_remove(qid, tid);               // remove from current queue
                        tcb_set_priority(tid, q + 1);          // boost priority
                        tqueue_enqueue(READY_QUEUE(q+1), tid);  // enqueue at higher level
                    }
                    }
                tid = next_tid;
            }
        }
        /* ----------------------------- */

        spinlock_release(&sched_lk);
        thread_yield();
    } else {
        spinlock_release(&sched_lk);
    }
}

void thread_sleep(void *chan, spinlock_t *lk)
{
    unsigned int curid = get_curid();
    unsigned int new_cur_pid;

    if (lk == 0)
        KERN_PANIC("sleep without lock");

    spinlock_acquire(&sched_lk);
    spinlock_release(lk);

    /* --------- NEW CPU ACCOUNTING --------- */

    /* Compute recent CPU usage */
    int recent_cpu = (TCBPool[curid].cpu_ticks * 100) / SCHED_SLICE;
    if (recent_cpu > 100)
        recent_cpu = 100;

    int old_score = TCBPool[curid].cpu_score;

    int new_score =
        (ALPHA * recent_cpu +
         (100 - ALPHA) * old_score) / 100;

    if (new_score < 0)
        new_score = 0;
    if (new_score > 100)
        new_score = 100;

    TCBPool[curid].cpu_score = new_score;

    /* Reset CPU tick counter */
    TCBPool[curid].cpu_ticks = 0;

    /* Adjust priority */
    int prio = tcb_get_priority(curid);

    if (new_score >= CPU_HIGH) {
        if (prio > MIN_PRIORITY)
            prio--;
    }
    else if (new_score <= CPU_LOW) {
        if (prio < MAX_PRIORITY)
            prio++;
    }

    tcb_set_priority(curid, prio);

    /* -------------------------------------- */

    tcb_set_state(curid, TSTATE_SLEEP);
    tcb_set_chan(curid, chan);

    new_cur_pid = ready_dequeue();
    if (new_cur_pid == NUM_IDS) {
        unsigned int idle_pid = idle_tid_for_cpu(get_pcpu_idx());

        set_curid(idle_pid);
        tcb_set_state(idle_pid, TSTATE_RUN);
        spinlock_release(&sched_lk);
        kctx_switch(curid, idle_pid);
    } else {
        TCBPool[new_cur_pid].waiting_time = 0;
        tcb_set_state(new_cur_pid, TSTATE_RUN);
        set_curid(new_cur_pid);

        spinlock_release(&sched_lk);
        kctx_switch(curid, new_cur_pid);
    }

    spinlock_acquire(&sched_lk);
    tcb_set_chan(curid, 0);
    spinlock_release(&sched_lk);

    spinlock_acquire(lk);
}

void thread_wakeup(void *chan)
{
    spinlock_acquire(&sched_lk);
    unsigned int pid;
    for (pid = 1; pid < NUM_IDS; ++pid) {
        if (tcb_get_chan(pid) == chan && tcb_get_state(pid) == TSTATE_SLEEP) {
            tcb_set_state(pid, TSTATE_READY);
            tcb_set_chan(pid, 0);
            ready_enqueue(pid, tcb_get_priority(pid));          // CHANGED
        }
    }
    spinlock_release(&sched_lk);
}

void thread_run_idle(void)
{
    unsigned int cpu_idx = get_pcpu_idx();
    unsigned int idle_pid = idle_tid_for_cpu(cpu_idx);

    set_curid(idle_pid);
    tcb_set_state(idle_pid, TSTATE_RUN);

    while (1) {
        unsigned int new_cur_pid;

        spinlock_acquire(&sched_lk);
        new_cur_pid = ready_dequeue();

        if (new_cur_pid == NUM_IDS) {
            /* Nothing local and nothing worth stealing yet. Stay idle. */
            spinlock_release(&sched_lk);
            pause();
            continue;
        }

        /* ready_dequeue() may return a local thread or a stolen one. */
        TCBPool[new_cur_pid].waiting_time = 0;
        tcb_set_state(new_cur_pid, TSTATE_RUN);
        set_curid(new_cur_pid);
        spinlock_release(&sched_lk);

        kctx_switch(idle_pid, new_cur_pid);

        set_curid(idle_pid);
        tcb_set_state(idle_pid, TSTATE_RUN);
    }
}

void thread_exit(void)
{
    unsigned int old_cur_pid;
    unsigned int new_cur_pid;

    spinlock_acquire(&sched_lk);

    old_cur_pid = get_curid();
    tcb_set_state(old_cur_pid, TSTATE_DEAD);
    // Do NOT set state to READY or enqueue - process is being terminated

    new_cur_pid = ready_dequeue();
    if (new_cur_pid == NUM_IDS) {
        unsigned int idle_pid = idle_tid_for_cpu(get_pcpu_idx());

        set_curid(idle_pid);
        tcb_set_state(idle_pid, TSTATE_RUN);
        spinlock_release(&sched_lk);
        kctx_switch(old_cur_pid, idle_pid);
        return;
    }

    TCBPool[new_cur_pid].waiting_time = 0;
    tcb_set_state(new_cur_pid, TSTATE_RUN);
    set_curid(new_cur_pid);

    spinlock_release(&sched_lk);
    kctx_switch(old_cur_pid, new_cur_pid);
    // Should never return here
}

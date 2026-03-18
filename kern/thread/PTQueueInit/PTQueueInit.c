#include "lib/x86.h"

#include "import.h"

static void tqueue_enqueue_at(unsigned int cpu_idx, unsigned int chid, unsigned int pid)
{
	unsigned int tail;

	tail = tqueue_get_tail_at(cpu_idx, chid);

	if (tail == NUM_IDS) {
		tcb_set_prev(pid, NUM_IDS);
		tcb_set_next(pid, NUM_IDS);
		tqueue_set_head_at(cpu_idx, chid, pid);
		tqueue_set_tail_at(cpu_idx, chid, pid);
	} else {
		tcb_set_next(tail, pid);
		tcb_set_prev(pid, tail);
		tcb_set_next(pid, NUM_IDS);
		tqueue_set_tail_at(cpu_idx, chid, pid);
	}
}

static unsigned int tqueue_dequeue_at(unsigned int cpu_idx, unsigned int chid)
{
	unsigned int head, next, pid;

	pid = NUM_IDS;
	head = tqueue_get_head_at(cpu_idx, chid);

	if (head != NUM_IDS) {
		pid = head;
		next = tcb_get_next(head);

		if (next == NUM_IDS) {
			tqueue_set_head_at(cpu_idx, chid, NUM_IDS);
			tqueue_set_tail_at(cpu_idx, chid, NUM_IDS);
		} else {
			tcb_set_prev(next, NUM_IDS);
			tqueue_set_head_at(cpu_idx, chid, next);
		}
		tcb_set_prev(pid, NUM_IDS);
		tcb_set_next(pid, NUM_IDS);
	}

	return pid;
}

static void tqueue_remove_at(unsigned int cpu_idx, unsigned int chid, unsigned int pid)
{
	unsigned int prev, next;

	prev = tcb_get_prev(pid);
	next = tcb_get_next(pid);

	if (prev == NUM_IDS) {
		if (next == NUM_IDS) {
			tqueue_set_head_at(cpu_idx, chid, NUM_IDS);
			tqueue_set_tail_at(cpu_idx, chid, NUM_IDS);
		} else {
			tcb_set_prev(next, NUM_IDS);
			tqueue_set_head_at(cpu_idx, chid, next);
		}
	} else {
		if (next == NUM_IDS) {
			tcb_set_next(prev, NUM_IDS);
			tqueue_set_tail_at(cpu_idx, chid, prev);
		} else {
			tcb_set_next(prev, next);
			tcb_set_prev(next, prev);
		}
	}
	tcb_set_prev(pid, NUM_IDS);
	tcb_set_next(pid, NUM_IDS);
}

static void ready_enqueue_on_cpu(unsigned int cpu_idx, unsigned int tid, int priority)
{
	if (priority > MAX_PRIORITY) priority = MAX_PRIORITY;
	if (priority < MIN_PRIORITY) priority = MIN_PRIORITY;

	/* Track which CPU currently owns this runnable thread in its ready queues. */
	tcb_set_priority(tid, priority);
	tcb_set_ready_cpu(tid, cpu_idx);
	tqueue_enqueue_at(cpu_idx, READY_QUEUE(priority), tid);
}

static unsigned int ready_dequeue_from_cpu(unsigned int cpu_idx)
{
	int prio;

	for (prio = MAX_PRIORITY; prio >= MIN_PRIORITY; prio--) {
		unsigned int tid = tqueue_dequeue_at(cpu_idx, READY_QUEUE(prio));
		if (tid != NUM_IDS) {
			tcb_set_ready_cpu(tid, -1);
			return tid;
		}
	}

	return NUM_IDS;
}

static unsigned int ready_queue_len_on_cpu(unsigned int cpu_idx)
{
	unsigned int total = 0;
	int prio;

	for (prio = MAX_PRIORITY; prio >= MIN_PRIORITY; prio--) {
		unsigned int tid = tqueue_get_head_at(cpu_idx, READY_QUEUE(prio));
		while (tid != NUM_IDS) {
			total++;
			tid = tcb_get_next(tid);
		}
	}

	return total;
}

static unsigned int ready_steal_from_busiest(void)
{
	unsigned int cpu_idx = get_pcpu_idx();
	unsigned int best_cpu = NUM_CPUS;
	unsigned int best_load = 0;
	unsigned int donor;

	/*
	 * Load balancing policy:
	 * if this CPU has no local runnable work, scan the other CPUs and pick
	 * the one with the largest number of ready threads as the donor.
	 */
	for (donor = 0; donor < NUM_CPUS; donor++) {
		unsigned int load;

		if (donor == cpu_idx)
			continue;

		load = ready_queue_len_on_cpu(donor);
		if (load > best_load) {
			best_load = load;
			best_cpu = donor;
		}
	}

	if (best_cpu == NUM_CPUS || best_load == 0)
		return NUM_IDS;

	/*
	 * Steal using the normal priority order of the donor CPU, so the stolen
	 * thread is still the highest-priority runnable thread on that CPU.
	 */
	return ready_dequeue_from_cpu(best_cpu);
}


/**
 * Initializes all the thread queues with
 * tqueue_init_at_id.
 */
void tqueue_init(unsigned int mbi_addr)
{
	unsigned int cpu_idx, chid;

	tcb_init(mbi_addr);

	chid = 0;
	cpu_idx = 0;
	while (cpu_idx < NUM_CPUS) {
		while (chid < NUM_IDS + NPRIO ) {
			tqueue_init_at_id(cpu_idx, chid);
			chid++;
		}
		chid = 0;
		cpu_idx++;
	}
}

/**
 * Insert the TCB #pid into the tail of the thread queue #chid.
 * Recall that the doubly linked list is index based.
 * So you only need to insert the index.
 * Hint: there are multiple cases in this function.
 */
void tqueue_enqueue(unsigned int chid, unsigned int pid)
{
	tqueue_enqueue_at(get_pcpu_idx(), chid, pid);
	dprintf("enqueue chid=%d pid=%d head=%d tail=%d\n",
	        chid, pid, tqueue_get_head(chid), tqueue_get_tail(chid));
}

/**
 * Reverse action of tqueue_enqueue, i.g., pops a TCB from the head of specified queue.
 * It returns the poped thread's id, or NUM_IDS if the queue is empty.
 * Hint: there are mutiple cases in this function.
 */
unsigned int tqueue_dequeue(unsigned int chid)
{
	return tqueue_dequeue_at(get_pcpu_idx(), chid);
}

/**
 * Removes the TCB #pid from the queue #chid.
 * Hint: there are many cases in this function.
 */
void tqueue_remove(unsigned int chid, unsigned int pid)
{
	tqueue_remove_at(get_pcpu_idx(), chid, pid);
}

void ready_enqueue(unsigned int tid, int priority)
{
    ready_enqueue_on_cpu(get_pcpu_idx(), tid, priority);
}

unsigned int ready_dequeue(void)
{
    unsigned int tid;

    /* Fast path: always prefer local runnable work first. */
    tid = ready_dequeue_from_cpu(get_pcpu_idx());
    if (tid != NUM_IDS)
        return tid;

    /* Slow path: local CPU is idle, so try to steal from the busiest CPU. */
    return ready_steal_from_busiest();
}

void ready_remove(unsigned int tid)
{
	int cpu_idx = tcb_get_ready_cpu(tid);

	if (cpu_idx < 0 || cpu_idx >= NUM_CPUS)
		return;

	tqueue_remove_at((unsigned int) cpu_idx, READY_QUEUE(tcb_get_priority(tid)), tid);
	tcb_set_ready_cpu(tid, -1);
}

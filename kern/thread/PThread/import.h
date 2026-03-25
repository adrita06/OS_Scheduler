#ifndef _KERN_THREAD_PTHREAD_IMPORT_H_
#define _KERN_THREAD_PTHREAD_IMPORT_H_

#define AGING_THRESHOLD 10
#define MAX_PRIORITY    9
#define MIN_PRIORITY    0
#define ALPHA           50
#define CPU_HIGH        75
#define CPU_LOW         25
#define SCHED_SLICE     10


#ifdef _KERN_

unsigned int kctx_new(void *entry, unsigned int id, unsigned int quota);
void kctx_switch(unsigned int from_pid, unsigned int to_pid);

void tcb_set_state(unsigned int pid, unsigned int state);
void* tcb_get_chan(unsigned int pid);
void tcb_set_chan(unsigned int pid, void *chan);

void tqueue_init(unsigned int mbi_addr);
void tqueue_enqueue(unsigned int chid, unsigned int pid);
unsigned int tqueue_dequeue(unsigned int chid);

void ready_enqueue(unsigned int tid, int priority);  // ADD
unsigned int ready_dequeue(void);                             // ADD

unsigned int get_curid(void);
void set_curid(unsigned int curid);

// Add this to import.h alongside the other tcb_ declarations:
int tcb_get_priority(unsigned int pid);
void tcb_set_priority(unsigned int pid, int prio);

unsigned int tcb_get_state(unsigned int pid);  // needed for thread_wakeup
unsigned int tqueue_get_head(unsigned int chid); // needed for aging loop in sched_update
unsigned int tcb_get_next(unsigned int pid);     // needed for aging loop
void tqueue_remove(unsigned int chid, unsigned int pid); // needed for aging loop
void pause(void);
int get_pcpu_idx(void);

#endif /* _KERN_ */

#endif /* !_KERN_THREAD_PTHREAD_H_ */

#ifndef _KERN_THREAD_PTQueueINIT_H_
#define _KERN_THREAD_PTQueueINIT_H_

#ifdef _KERN_

#define MAX_PRIORITY 9
#define MIN_PRIORITY 0
#define READY_QUEUE(p)  (NUM_IDS + (p))
#define NPRIO 10

unsigned int tcb_get_prev(unsigned int pid);
void tcb_set_prev(unsigned int pid, unsigned int prev_pid);
unsigned int tcb_get_next(unsigned int pid);
void tcb_set_next(unsigned int pid, unsigned int next_pid);
void tcb_init(unsigned int mbi_addr);

void tcb_set_priority(unsigned int pid, int priority);
int tcb_get_priority(unsigned int pid);

unsigned int tqueue_get_head(unsigned int chid);
void tqueue_set_head(unsigned int chid, unsigned int head);
unsigned int tqueue_get_tail(unsigned int chid);
void tqueue_set_tail(unsigned int chid, unsigned int tail);
void tqueue_init_at_id(unsigned int cpu_idx, unsigned int chid);
#endif /* _KERN_ */

#endif /* !_KERN_THREAD_PTQueueINIT_H_ */

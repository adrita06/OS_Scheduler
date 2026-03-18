#include <lib/debug.h>
#include <lib/types.h>
#include <lib/monitor.h>
#include <thread/PThread/export.h>
#include <thread/PTCBIntro/export.h>
#include <thread/PTQueueInit/export.h>
#include <thread/PCurID/export.h>
#include <dev/devinit.h>
#include <pcpu/PCPUIntro/export.h>
#include <proc/PProc/export.h>
#include <lib/kstack.h>
#include <lib/thread.h>
#include <lib/x86.h>

#define NUM_CPUS 8
#define NUM_IDS 64

/* Global definitions for kernel stacks (declared extern in kstack.h) */
struct kstack bsp_kstack[NUM_CPUS];
struct kstack proc_kstack[NUM_IDS];

#ifdef TEST
extern bool test_PKCtxNew(void);
extern bool test_PTCBInit(void);
extern bool test_PTQueueInit(void);
extern bool test_PThread(void);
#endif

static volatile int cpu_booted = 0;
static volatile int all_ready = FALSE;
static void kern_main_ap(void);

uint32_t pcpu_ncpu(void);
int pcpu_boot_ap(uint32_t cpu_idx, void (*f)(void), uintptr_t stack_addr);
void kctx_switch(unsigned int from_pid, unsigned int to_pid);

extern uint8_t _binary___obj_user_idle_idle_start[];
extern uint8_t _binary___obj_user_pingpong_ding_start[];
extern uint8_t _binary___obj_user_shell_shell_start[];

static void
kern_main (void)
{
    KERN_INFO("[BSP KERN] In kernel main.\n\n");

    KERN_INFO("[BSP KERN] Number of CPUs in this system: %d. \n", pcpu_ncpu());

    int cpu_idx = get_pcpu_idx();
    unsigned int pid;

    int i;
    all_ready = FALSE;
    for (i = 1; i < pcpu_ncpu(); i++){
        KERN_INFO("[BSP KERN] Boot CPU %d .... \n", i);

        bsp_kstack[i].cpu_idx = i;
        pcpu_boot_ap(i, kern_main_ap, (uintptr_t) &(bsp_kstack[i]));

        while (get_pcpu_boot_info(i) == FALSE);

        KERN_INFO("[BSP KERN] done.\n");

    }

   #ifdef TEST
    dprintf("------------------\n");
    dprintf("| PKCtxNew TESTS |\n");
    dprintf("------------------\n");
    if (test_PKCtxNew()) {
        KERN_INFO("PKCtxNew test passed.\n");
    } else {
        KERN_INFO("PKCtxNew test failed.\n");
    }
    dprintf("------------------\n");
    dprintf("| PTCBInit TESTS |\n");
    dprintf("------------------\n");
    if (test_PTCBInit()) {
        KERN_INFO("PTCBInit test passed.\n");
    } else {
        KERN_INFO("PTCBInit test failed.\n");
    }
    dprintf("------------------\n");
    dprintf("| PTQueueInit TESTS |\n");
    dprintf("------------------\n");
    if (test_PTQueueInit()) {
        KERN_INFO("PTQueueInit test passed.\n");
    } else {
        KERN_INFO("PTQueueInit test failed.\n");
    }

    dprintf("------------------\n");
    dprintf("| PThread TESTS |\n");
    dprintf("------------------\n");
    if (test_PThread()) {
        KERN_INFO("PThread test passed.\n");
    } else {
        KERN_INFO("PThread test failed.\n");
    }
    KERN_INFO("All thread tests completed.\n");
    #endif

    pid = proc_create (_binary___obj_user_idle_idle_start, 1000);
    pid = proc_create (_binary___obj_user_shell_shell_start, 1000);
    KERN_INFO("CPU%d: process shell %d is created.\n", cpu_idx, pid);

    /*
     * Preserve the original BSP startup behavior:
     * CPU0 enters the shell directly, while the scheduler's per-CPU idle
     * loops are still available for load balancing when a CPU has no work.
     */
    ready_remove(pid);
    tcb_set_state(pid, TSTATE_RUN);
    set_curid(pid);

    all_ready = TRUE;
    kctx_switch(0, pid);
    KERN_PANIC("kern_main() should never reach here.\n");
}

static void
kern_main_ap(void)
{
    int cpu_idx = get_pcpu_idx();
    unsigned int pid, pid2;

    set_pcpu_boot_info(cpu_idx, TRUE);

    while (all_ready == FALSE);

    KERN_INFO("[AP%d KERN] kernel_main_ap\n", cpu_idx);

    cpu_booted ++;
    thread_run_idle();
}

void
kern_init (uintptr_t mbi_addr)
{
    thread_init(mbi_addr);

    KERN_INFO("[BSP KERN] Kernel initialized.\n");

    kern_main ();
}

void
kern_init_ap(void (*f)(void))
{
	devinit_ap();
	f();
}

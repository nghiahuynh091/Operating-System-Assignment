
#include "queue.h"
#include "sched.h"
#include "libmem.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
static struct queue_t ready_queue;
static struct queue_t run_queue; //! Obsolete
pthread_mutex_t queue_lock;
pthread_mutex_t running_list_lock;

static struct queue_t running_list; //! The list of all running processes (track by pid - no duplicates)
#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if(!empty(&mlq_ready_queue[prio])) 
			return -1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
    int i ;

	for (i = 0; i < MAX_PRIO; i ++) {
		mlq_ready_queue[i].size = 0;
		slot[i] = MAX_PRIO - i; 
		mlq_ready_queue[i].curr_slot = slot[i];
	}
#endif
	ready_queue.size = 0;
	run_queue.size = 0;
	pthread_mutex_init(&queue_lock, NULL);
	pthread_mutex_init(&running_list_lock, NULL);
}

void add_proc_to_running_list(struct pcb_t * proc) {
	pthread_mutex_lock(&running_list_lock);
	// Check if the proc is in the running list, if not, add it
	if (empty(&running_list))
	{
		enqueue(&running_list, proc);
	}
	else
	{
		struct pcb_t * running_proc;
		// Go through the running list to check if the proc is already there
		// If found, do not add it again
		for (int i = 0; i < running_list.size; i++)
		{
			running_proc = running_list.proc[i];
			if(running_proc->pid == proc->pid)
			{
				pthread_mutex_unlock(&running_list_lock);
				return; // Already in the running list, no need to add
			}
		}
		// If not found, add it to the running list
		enqueue(&running_list, proc);
	}
	pthread_mutex_unlock(&running_list_lock);
}

// Dequeue a specific process from the running list
void put_process_out_of_runninglist(struct pcb_t * proc) {
	pthread_mutex_lock(&running_list_lock);
	dequeue_specific(&running_list, proc);
	pthread_mutex_unlock(&running_list_lock);
}
#ifdef MLQ_SCHED
/* 
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
struct pcb_t *get_mlq_proc(void) {
    struct pcb_t *proc = NULL;
    pthread_mutex_lock(&queue_lock);
    unsigned long prio;
    int proc_found = 0;
    int ready_queue_found = -1;

    for (prio = 0; prio < MAX_PRIO; prio++) 
	{
        if (!empty(&mlq_ready_queue[prio])) 
		{
            if (ready_queue_found == -1) 
			{
                ready_queue_found = prio;
            }
            if (mlq_ready_queue[prio].curr_slot > 0) 
			{
                proc = dequeue(&mlq_ready_queue[prio]);
                if (proc != NULL) 
				{
					proc_found = 1;
					mlq_ready_queue[prio].curr_slot--;
					break;
				}
			}
		}
	}
	
	if (ready_queue_found == -1)
	{
		// No process in ready queue
		pthread_mutex_unlock(&queue_lock);
		return NULL;
	}
	else
	{
		if (proc_found == 0) // No process is able to run as all curr_slot is <=0
		{
			// Reset all curr_slot to MAX_PRIO - prio
			for (prio = 0; prio < MAX_PRIO; prio++)
			{
				mlq_ready_queue[prio].curr_slot = MAX_PRIO - prio;
			}
			proc = dequeue(&mlq_ready_queue[ready_queue_found]);
			if (proc != NULL)
			{
				mlq_ready_queue[ready_queue_found].curr_slot--;
			}
		}
	}
	// printf("\033[31mDEBUG\033[0m LIST OF RUNNING PROCESSES\n");
	// printf("Running list size: %d\n", running_list.size);
	// for(int i = 0; i < running_list.size; i++)
	// {
	// 	struct pcb_t * running_proc = running_list.proc[i];
	// 	printf("\033[31mDEBUG\033[0m PID: %d\n", running_proc->pid);
	// }
	pthread_mutex_unlock(&queue_lock);
	return proc;	
}

void put_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);	
}

struct pcb_t * get_proc(void) {
	return get_mlq_proc();
}
// Called by CPU to put a process back to ready queue
void put_proc(struct pcb_t * proc) {
	proc->ready_queue = &ready_queue;
	proc->mlq_ready_queue = mlq_ready_queue;
	proc->running_list = & running_list;

	add_proc_to_running_list(proc);
	
	return put_mlq_proc(proc);
}
// Called by loader to add a new process
void add_proc(struct pcb_t * proc) {
	proc->ready_queue = &ready_queue;
	proc->mlq_ready_queue = mlq_ready_queue;
	proc->running_list = & running_list;

	add_proc_to_running_list(proc);

	return add_mlq_proc(proc);
}

#else
struct pcb_t * get_proc(void) {
	struct pcb_t * proc = NULL;
	/*TODO: get a process from [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
	return proc;
}

void put_proc(struct pcb_t * proc) {
	proc->ready_queue = &ready_queue;
	proc->running_list = & running_list;

	/* TODO: put running proc to running_list */

	pthread_mutex_lock(&queue_lock);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
	proc->ready_queue = &ready_queue;
	proc->running_list = & running_list;

	/* TODO: put running proc to running_list */

	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);	
}
#endif



#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        /* TODO: put a new process to queue [q] */
        if (q == NULL || proc == NULL) return;
        if (q->size > MAX_QUEUE_SIZE - 1) {
                printf("Queue is full, cannot enqueue\n");
                exit(1);
        }

        q->proc[q->size] = proc;
        q->size++;
}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose priority is the highest
        * in the queue [q] and remember to remove it from q
        * */
       if (q == NULL || q->size == 0) return NULL;

       /* Find the process with the highest priority */
       int highest_prio_index = 0;
       for (int i = 1; i < q->size; i++) {
               if (q->proc[i]->prio < q->proc[highest_prio_index]->prio) {
                       highest_prio_index = i;
               }
       }

       struct pcb_t * proc = q->proc[highest_prio_index];

       /* Shift the remaining processes */
       for (int i = highest_prio_index; i < q->size - 1; i++) {
               q->proc[i] = q->proc[i + 1];
       }

       q->proc[q->size - 1] = NULL;
       q->size--;
       return proc;
}

//Dequeue a specific process from the queue
struct pcb_t * dequeue_specific(struct queue_t * q, struct pcb_t * proc) {
        if (q == NULL || proc == NULL) return NULL;

        for (int i = 0; i < q->size; i++) {
                if(q->proc[i] == NULL) continue;
                /* Check if the process matches */
                if (q->proc[i]->pid == proc->pid) {
                        struct pcb_t * removed_proc = q->proc[i];

                        /* Shift the remaining processes */
                        for (int j = i; j < q->size - 1; j++) {
                                q->proc[j] = q->proc[j + 1];
                        }

                        q->proc[q->size - 1] = NULL;
                        q->size--;
                        return removed_proc;
                }
        }
        return NULL;
}
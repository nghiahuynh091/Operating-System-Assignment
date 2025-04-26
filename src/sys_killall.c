/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */
#include "common.h"
#include "syscall.h"
#include "stdio.h"
#include "libmem.h"
#include "queue.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
// #include <linux/uaccess.h>

extern pthread_mutex_t queue_lock;
extern pthread_mutex_t running_list_lock;
#include "sched.h"

// Single function to free a process's resources
void free_pcb(struct pcb_t *proc)
{
    if (!proc)
        return;

    if (proc->mm)
    {
        // First free physical memory frames
        free_pcb_memph(proc); // This is defined in libmem.c

        // Free the page table
        if (proc->mm->pgd)
            free(proc->mm->pgd);

        // Free FIFO page list
        struct pgn_t *curr_pg = proc->mm->fifo_pgn;
        while (curr_pg)
        {
            struct pgn_t *next = curr_pg->pg_next;
            free(curr_pg);
            curr_pg = next;
        }

        // Free all VMAs and their regions
        struct vm_area_struct *curr_vma = proc->mm->mmap;
        while (curr_vma)
        {
            // Free the regions in this VMA
            struct vm_rg_struct *curr_rg = curr_vma->vm_freerg_list;
            while (curr_rg)
            {
                struct vm_rg_struct *next_rg = curr_rg->rg_next;
                free(curr_rg);
                curr_rg = next_rg;
            }

            // Free the VMA itself
            struct vm_area_struct *next_vma = curr_vma->vm_next;
            free(curr_vma);
            curr_vma = next_vma;
        }

        // Free the memory manager
        free(proc->mm);
    }

    // Finally free the PCB
    free(proc);
}

int __sys_killall(struct pcb_t *caller, struct sc_regs *regs)
{
    char proc_name[100];
    uint32_t data;
    uint32_t memrg = regs->a1;

    // Read process name from memory
    int i = 0;
    data = 0;
    while (data != -1 && i < 50) {
        if (libread(caller, memrg, i, &data) != 0) {
            printf("DEBUG: Failed to read memory at index %d\n", i);
            proc_name[i] = '\0';
            break;
        }
        proc_name[i] = data;
        if (data == -1)
            proc_name[i] = '\0';
        i++;
    }
    proc_name[i] = '\0';

    printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);

    int terminated_count = 0;

    // Check running_list
    pthread_mutex_lock(&running_list_lock);
    if (caller->running_list != NULL) {
        for (int j = 0; j < caller->running_list->size;) {
            struct pcb_t *proc = caller->running_list->proc[j];
            if (proc == NULL) {
                j++;
                continue;
            }
            // Construct basename from path
            const char *basename = proc->path;
            const char *slash = strrchr(proc->path, '/');
            if (!slash)
                slash = strrchr(proc->path, '\\');
            if (slash)
                basename = slash + 1;

            if (strcmp(basename, proc_name) == 0) {
                if (proc->pid == caller->pid) {
                    printf("Skipping self-termination for process: %s (PID: %d)\n", basename, proc->pid);
                    j++; // Move to the next process
                    continue;
                }
                printf("Found match in running_list: %s (PID: %d)\n", basename, proc->pid);
                proc->state = TERMINATED; // Đánh dấu để kết thúc
                terminated_count++;
                // Xóa khỏi running_list
                // for (int k = j; k < caller->running_list->size - 1; k++) {
                //     caller->running_list->proc[k] = caller->running_list->proc[k + 1];
                // }
                // dequeue_specific(caller->running_list, proc);
                // caller->running_list->size--;
                j++;
            } else {
                j++;
            }
        }
    }
    else
    {
        printf("No process in running_list\n");
    }
    pthread_mutex_unlock(&running_list_lock);

    // Check mlq_ready_queue
    // pthread_mutex_lock(&queue_lock);
    // if (caller->mlq_ready_queue != NULL) {
    //     printf("Checking mlq_ready_queue...\n");
    //     for (int prio = 0; prio < MAX_PRIO; prio++) {
    //         for (int j = 0; j < caller->mlq_ready_queue[prio].size;) {
    //             struct pcb_t *proc = caller->mlq_ready_queue[prio].proc[j];
    //             if (proc == NULL) {
    //                 j++;
    //                 continue;
    //             }
    //             const char *basename = proc->path;
    //             const char *slash = strrchr(proc->path, '/');
    //             if (!slash)
    //                 slash = strrchr(proc->path, '\\');
    //             if (slash)
    //                 basename = slash + 1;

    //             if (strcmp(basename, proc_name) == 0) {
    //                 printf("Found match in mlq_ready_queue: %s (PID: %d)\n", basename, proc->pid);
    //                 proc->state = TERMINATED;
    //                 terminated_count++;
    //                 // Xóa khỏi mlq_ready_queue
    //                 for (int k = j; k < caller->mlq_ready_queue[prio].size - 1; k++) {
    //                     caller->mlq_ready_queue[prio].proc[k] =
    //                         caller->mlq_ready_queue[prio].proc[k + 1];
    //                 }
    //                 caller->mlq_ready_queue[prio].size--;
    //             } else {
    //                 j++;
    //             }
    //         }
    //     }
    // }
    // pthread_mutex_unlock(&queue_lock);

    printf(">>>>>> Total sended signal TERMINATED: %d <<<<<<\n", terminated_count);
    return 0;
}
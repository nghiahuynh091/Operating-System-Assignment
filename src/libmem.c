/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */
// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c
 */

#include "string.h"
#include "mm.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;
  struct vm_rg_struct *prev_node = NULL;
  struct vm_rg_struct *merge_candidate = NULL;
  struct vm_rg_struct * freeRG = malloc(sizeof(struct vm_rg_struct));
  freeRG->rg_start = rg_elmt->rg_start;
  freeRG->rg_end = rg_elmt->rg_end;

  int merged = 0;

  if (freeRG->rg_start >= freeRG->rg_end)
    return -1;

  // If the list is empty, just add the element
  if (rg_node == NULL)
  {
    mm->mmap->vm_freerg_list = freeRG;
    freeRG->rg_next = NULL;
    return 0;
  }

  // First pass: look for a region where our new region connects to the end
  while (rg_node)
  {
    if (rg_node->rg_end == freeRG->rg_start)
    {
      // Found a region that ends where our new region starts
      rg_node->rg_end = freeRG->rg_end;
      merge_candidate = rg_node;
      merged = 1;
      break;
    }
    else if (rg_node->rg_start == freeRG->rg_end)
    {
      // Found a region that starts where our new region ends
      rg_node->rg_start = freeRG->rg_start;
      merge_candidate = rg_node;
      merged = 1;
      break;
    }
    prev_node = rg_node;
    rg_node = rg_node->rg_next;
  }

  // If we merged, check if this new expanded region can merge with another
  if (merged)
  {
    rg_node = mm->mmap->vm_freerg_list;
    prev_node = NULL;

    while (rg_node)
    {
      if (rg_node != merge_candidate)
      { // Don't compare with itself
        if (rg_node->rg_start == merge_candidate->rg_end)
        {
          // The expanded region's end matches another region's start
          merge_candidate->rg_end = rg_node->rg_end;

          // Remove the other region from the list
          if (prev_node)
            prev_node->rg_next = rg_node->rg_next;
          else
            mm->mmap->vm_freerg_list = rg_node->rg_next;

          // Free the removed node if needed
          // free(rg_node);  // Uncomment if you need to free memory

          return 0;
        }
        else if (rg_node->rg_end == merge_candidate->rg_start)
        {
          // The expanded region's start matches another region's end
          merge_candidate->rg_start = rg_node->rg_start;

          // Remove the other region from the list
          if (prev_node)
            prev_node->rg_next = rg_node->rg_next;
          else
            mm->mmap->vm_freerg_list = rg_node->rg_next;

          // Free the removed node if needed
          // free(rg_node);  // Uncomment if you need to free memory

          return 0;
        }
      }

      prev_node = rg_node;
      rg_node = rg_node->rg_next;
    }

    return 0; // We merged but didn't find a second region to merge with
  }

  // No merging was possible, add to the end of the list
  freeRG->rg_next = NULL;

  if (prev_node != NULL)
  {
    prev_node->rg_next = freeRG;
  }
  else
  {
    mm->mmap->vm_freerg_list = freeRG;
  }

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  // printf("DEBUG: all rgid in symrgtbl: ");
  // for (int i = 0; i < PAGING_MAX_SYMTBL_SZ; i++)
  // {
  //   printf("%ld", mm->symrgtbl[i].rg_start);
  //   printf("-");
  //   printf("%ld ", mm->symrgtbl[i].rg_end);
  // }

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
  /*Allocate at the toproof */
  struct vm_rg_struct rgnode;

  /* TODO: commit the vmaid */
  // rgnode.vmaid
  //! 1. Check the free region list first
  pthread_mutex_unlock(&mmvm_lock);

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
    // caller->mm->mmap->sbrk += size;
    *alloc_addr = rgnode.rg_start;

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

  /* TODO get_free_vmrg_area FAILED handle the region management (Fig.6)*/
  /* TODO retrive current vma if needed, current comment out due to compiler redundant warning*/
  /*Attempt to increate limit to get space */
  pthread_mutex_unlock(&mmvm_lock);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  //! 2. If cant find free region in the list, try to allocate in the heap (above sbrk and below vm_end)

  if (cur_vma == NULL)
    return -1;
  if (cur_vma->sbrk + size < cur_vma->vm_end) // If there is enough space in the heap
  {
    caller->mm->symrgtbl[rgid].rg_start = cur_vma->sbrk;
    caller->mm->symrgtbl[rgid].rg_end = cur_vma->sbrk + size;
    *alloc_addr = cur_vma->sbrk;
    cur_vma->sbrk += size;

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }
  //! 3. The heap is full, try to increase the limit (more page for the vmaid)
  //* inc_sz will be minus the size of the left space in vm_end
  int inc_sz = PAGING_PAGE_ALIGNSZ(size - (cur_vma->vm_end - cur_vma->sbrk));

  /* TODO retrive old_sbrk if needed, current comment out due to compiler redundant warning*/
  int old_sbrk = cur_vma->sbrk;

  /* TODO INCREASE THE LIMIT as inovking systemcall
   * sys_memap with SYSMEM_INC_OP
   */
  struct sc_regs *regs = malloc(sizeof(struct sc_regs));
  regs->a1 = SYSMEM_INC_OP;
  regs->a2 = vmaid;
  regs->a3 = inc_sz;
  uint32_t nghiasys = 17;
  /* SYSCALL 17 sys_memmap */
  int checking = inc_vma_limit(caller, vmaid, inc_sz);
  //  = syscall(caller, nghiasys, regs);
  // printf("checking alloc:%d", checking);
  if (checking < 0)
  {
    // print_pgtbl(caller, 0, -1);
    return -1;
  };

  rgnode.rg_start = old_sbrk;
  rgnode.rg_end = old_sbrk + size;

  /* TODO: commit the limit increment */

  caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
  caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
  cur_vma->sbrk += size;

  /* TODO: commit the allocation address */
  *alloc_addr = rgnode.rg_start;

  return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  struct vm_rg_struct *rgnode;

  // Dummy initialization for avoding compiler dummay warning
  // in incompleted TODO code rgnode will overwrite through implementing
  // the manipulation of rgid later

  if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
    return -1;

  /* TODO: Manage the collect freed region to freerg_list */
  rgnode = get_symrg_byid(caller->mm, rgid);

  /*enlist the obsoleted memory region */
  enlist_vm_freerg_list(caller->mm, rgnode);

  return 0;
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  /* TODO Implement allocation on vm area 0 */
  int addr;

  /* By default using vmaid = 0 */
  int result = __alloc(proc, 0, reg_index, size, &addr);

#ifdef IODUMP
  if (result == 0)
  {
    pthread_mutex_lock(&mmvm_lock);
    printf("===== PHYSICAL MEMORY AFTER ALLOCATION =====\n");
    printf("PID=%d - Region=%d - Address=%08x - Size=%d byte\n", proc->pid, reg_index, addr, size);
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1); // Print the page table
#endif
    // MEMPHY_dump(proc->mram); // Dump the physical memory
    printf("================================================================\n");
    pthread_mutex_unlock(&mmvm_lock);
  }
#endif

  return result;
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int libfree(struct pcb_t *proc, uint32_t reg_index)
{

  /* By default using vmaid = 0 */
  int result = __free(proc, 0, reg_index);

#ifdef IODUMP
  if (result == 0)
  {
    pthread_mutex_lock(&mmvm_lock);
    printf("===== PHYSICAL MEMORY AFTER DEALLOCATION =====\n");
    printf("PID=%d - Region=%d\n", proc->pid, reg_index);
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1); // Print the page table
#endif
    printf("===================LIST OF FREE REGION===================\n");
    print_list_rg(proc->mm->mmap->vm_freerg_list);
    // MEMPHY_dump(proc->mram); // Dump the physical memory
    printf("================================================================\n");
    pthread_mutex_unlock(&mmvm_lock);
  }
#endif

  return result;
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  if (pgn < 0 || pgn >= PAGING_MAX_PGN)
  {
    printf("Invalid page number %d\n", pgn);
    return -1;
  }
  uint32_t pte = mm->pgd[pgn];

  if (!PAGING_PAGE_PRESENT(pte))
  { /* Page is not online, make it actively living */
    printf("Page %d is not present\n", pgn);
    printf("!!!!!!!!!!======== PAGE FAULT ========!!!!!!!!!!\n");
    return -1; //! Return as no page replacement is needed
  }

  *fpn = PAGING_FPN(mm->pgd[pgn]);

  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  /* TODO
   *  MEMPHY_read(caller->mram, phyaddr, data);
   *  MEMPHY READ
   *  SYSCALL 17 sys_memmap with SYSMEM_IO_READ
   */
  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  uint32_t nghiasys = 17;

  struct sc_regs *regs = malloc(sizeof(struct sc_regs));
  regs->a1 = SYSMEM_IO_READ;
  regs->a2 = phyaddr;
  regs->a3 = *data;
  syscall(caller, nghiasys, regs);
  /* SYSCALL 17 sys_memmap */
  // MEMPHY_read(caller->mram,phyaddr, data);
  // Update data
  *data = regs->a3;

  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  /* TODO
   *  MEMPHY_write(caller->mram, phyaddr, value);
   *  MEMPHY WRITE
   *  SYSCALL 17 sys_memmap with SYSMEM_IO_WRITE
   */
  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  uint32_t nghiasys = 17;

  struct sc_regs *regs = malloc(sizeof(struct sc_regs));
  regs->a1 = SYSMEM_IO_WRITE;
  regs->a2 = phyaddr;
  regs->a3 = value;
  syscall(caller, nghiasys, regs);
  // MEMPHY_write(caller->mram,phyaddr, value);

  /* SYSCALL 17 sys_memmap */

  // Update data
  // data = (BYTE)

  return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  return pg_getval(caller->mm, currg->rg_start + offset, data, caller);
}

/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    uint32_t offset,    // Source address = [source] + [offset]
    uint32_t *destination)
{
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);
/* TODO update result of reading action*/
// destination
#ifdef IODUMP
  if (val == 0)
  {
    pthread_mutex_lock(&mmvm_lock);
    printf("===== PHYSICAL MEMORY AFTER READING =====\n");
    printf("PID=%d read region=%d offset=%d value=%d\n", proc->pid, source, offset, data);
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1); // Print the page table
#endif
    printf("LIST OF FREE REGION: ");
    print_list_rg(proc->mm->mmap->vm_freerg_list); // Print the list of free regions
    printf("================================================================\n");
    printf("===== PHYSICAL MEMORY DUMP =====\n");
    // int addr = (source << PAGING_ADDR_FPN_LOBIT) + offset;
    // printf("BYTE %08x: %d\n", addr, data);
    MEMPHY_dump(proc->mram); // Dump the physical memory
    printf("===== PHYSICAL MEMORY END-DUMP =====\n");
    printf("================================================================\n");
    pthread_mutex_unlock(&mmvm_lock);
  }
#endif

  *destination = data; // Update the destination with the read value
  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
  {
    printf("Invalid memory identify\n");
    return -1;
  }
  return pg_setval(caller->mm, currg->rg_start + offset, value, caller);
}

/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    uint32_t offset)
{
  int result = __write(proc, 0, destination, offset, data);

#ifdef IODUMP
  if (result == 0)
  {
    pthread_mutex_lock(&mmvm_lock);
    printf("===== PHYSICAL MEMORY AFTER WRITING =====\n");
    printf("PID=%d write region=%d offset=%d value=%d\n", proc->pid, destination, offset, data);
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1); // Print the page table
#endif
    printf("================================================================\n");
    printf("===== PHYSICAL MEMORY DUMP =====\n");
    // int addr = (destination << PAGING_ADDR_FPN_LOBIT) + offset;
    // printf("BYTE %08x: %d\n", addr, data);
    MEMPHY_dump(proc->mram); // Dump the physical memory
    printf("===== PHYSICAL MEMORY END-DUMP =====\n");
    printf("================================================================\n");
    pthread_mutex_unlock(&mmvm_lock);
  }
#endif

  return result;
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte = caller->mm->pgd[pagenum];

    if (!PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_PTE_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    }
    else
    {
      fpn = PAGING_PTE_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);
    }
  }

  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (cur_vma == NULL)
    return -1;
  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

  if (rgit == NULL)
    return -1;

  /* Probe unintialized newrg */
  newrg->rg_start = newrg->rg_end = -1;

  /* TODO Traverse on list of free vm region to find a fit space */
  while (rgit != NULL)
  {
    if (rgit->rg_start + size <= rgit->rg_end)
    { /* Current region has enough space */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;

      /* Update left space in chosen region */
      if (rgit->rg_start + size < rgit->rg_end)
      {
        rgit->rg_start = rgit->rg_start + size;
      }
      else
      { /* Use up all space, remove current node */
        struct vm_rg_struct *nextrg = rgit->rg_next;

        /* Cloning */
        if (nextrg != NULL)
        {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;

          rgit->rg_next = nextrg->rg_next;

          free(nextrg);
        }
        else
        {                                /* End of free list */
          rgit->rg_start = rgit->rg_end; // dummy, size 0 region
          rgit->rg_next = NULL;
        }
      }
      break;
    }
    else
    {
      rgit = rgit->rg_next; // Traverse next rg
    }
  }

  if (newrg->rg_start == -1) // new region not found
  {
    return -1;
  }
  return 0;
}

// #endif

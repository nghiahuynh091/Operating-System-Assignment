// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
// struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
// {
//   struct vm_area_struct *pvma = mm->mmap;

//   if (mm->mmap == NULL)
//     return NULL;

//   int vmait = pvma->vm_id;

//   while (vmait < vmaid)
//   {
//     if (pvma == NULL)
//       return NULL;

//     pvma = pvma->vm_next;
//     vmait = pvma->vm_id;
//   }

//   return pvma;
// }
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  // printf("Enter get vma \n");
  struct vm_area_struct *pvma = mm->mmap;

  if (mm->mmap == NULL)
    return NULL;

  int vmait = 0;

  while (vmait < vmaid)
  {
    if (pvma == NULL)
      return NULL;

    vmait++;
    pvma = pvma->vm_next;
  }
  // printf("End get vma \n");
  return pvma;
}

int __mm_swap_page(struct pcb_t *caller, int vicfpn, int swpfpn)
{
  __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
  return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
{
  struct vm_rg_struct *newrg;
  /* TODO retrive current vma to obtain newrg, current comment out due to compiler redundant warning*/
  // struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  newrg = malloc(sizeof(struct vm_rg_struct));

  /* TODO: update the newrg boundary
  // newrg->rg_start = ...
  // newrg->rg_end = ...
  */
  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = cur_vma->sbrk + alignedsz;
  // newrg->rg_next = NULL;

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend)
{
  /* TODO validate the planned memory area is not overlapped */
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  /* TODO IMPLEMENTED: validate the planned memory area is not overlapped */

  struct vm_area_struct *vmait; // iterator

  for (vmait = caller->mm->mmap; vmait != NULL; vmait = vmait->vm_next)
  {

    if (vmait == cur_vma)
      continue; // check other vma

    // check overlap
    if (!OVERLAP(vmastart, vmaend, vmait->vm_start, vmait->vm_end))
      continue;

    // have only 1 endpoint overlapped
    if (vmastart == vmait->vm_end && vmaend != vmait->vm_start)
      continue;
    if (vmaend == vmait->vm_start && vmastart != vmait->vm_end)
      continue;

    // overlap happen
    return -1;
  }

  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz)
{
  struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
  int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  int incnumpage = inc_amt / PAGING_PAGESZ;
  struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  int old_end = cur_vma->vm_end;

  /*Validate overlap of obtained region */
  if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0)
    return -1; /*Overlap and failed allocation */

  /* TODO: Obtain the new vm area based on vmaid */
  // cur_vma->vm_end...
  //  inc_limit_ret...
  cur_vma->vm_end += inc_amt;
  if (vm_map_ram(caller, area->rg_start, area->rg_end, old_end, incnumpage, newrg) < 0){
    printf("Error: Unable to map memory\n");
    cur_vma->vm_end = old_end; // Restore the original end
    return -1; /* Map the memory to MEMRAM */
  }

  return 0;
}

// #endif

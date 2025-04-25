#include "common.h"
#include "syscall.h"
#include "stdio.h"

int __sys_xxxhandler(struct pcb_t *caller, struct sc_regs* reg)
{
    /*Todo: implement system calljob*/
    printf("The first system call parameter %d\n", reg->a1);
    return 0;
}

/**
 * ARM architecture common support functions
 *
 * Copyright (C) 2008, 2009  Heiko Will <hwill@inf.fu-berlin.de>
 * Copyright (C) 2009 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 * @ingroup cpu_arm7_common
 * @{
 * @file
 * @author Kaspar Schleiser <kaspar@schleiser.de>
 * @author Heiko Will <heiko.will@fu-berlin.de>
 * @}
 */

#include <stdio.h>
#include "arm_cpu.h"
#include "irq.h"
#include "sched.h"
#include "thread.h"

#define STACK_MARKER    (0x77777777)
#define REGISTER_CNT    (12)

__attribute__((used, section(".usr_stack"))) uint8_t usr_stack[USR_STACKSIZE];
__attribute__((used, section(".und_stack"))) uint8_t und_stack[UND_STACKSIZE];
__attribute__((used, section(".fiq_stack"))) uint8_t fiq_stack[FIQ_STACKSIZE];
__attribute__((used, section(".irq_stack"))) uint8_t irq_stack[ISR_STACKSIZE];
__attribute__((used, section(".abt_stack"))) uint8_t abt_stack[ABT_STACKSIZE];
__attribute__((used, section(".svc_stack"))) uint8_t svc_stack[ISR_STACKSIZE];

void thread_yield_higher(void)
{
    if (irq_is_in()) {
        sched_context_switch_request = 1;
    }
    else {
        __asm__("svc 0\n");
    }
}

/*----------------------------------------------------------------------------
 * Processor specific routine - here for ARM7
 * sizeof(void*) = sizeof(int)
 *--------------------------------------------------------------------------*/
char *thread_stack_init(thread_task_func_t task_func, void *arg, void *stack_start, int stack_size)
{
    unsigned int *stk;
    int i;
    stk = (unsigned int *)((uintptr_t)stack_start + stack_size);
    stk--;

    *stk = STACK_MARKER;

    /* set the return address (LR) */
    stk--;
    *stk = (unsigned int) sched_task_exit;

    /* set the stack pointer (SP) */
    stk--;
    *stk = (unsigned int)((unsigned int)stack_start + stack_size) - 4;

    /* build base stack */
    for (i = REGISTER_CNT; i > 0 ; i--) {
        stk--;
        *stk = i;
    }

    /* set argument to task_func */
    stk--;
    *stk = ((unsigned int) arg);

    /* set the entry point */
    stk--;
    *stk = ((unsigned int) task_func);
    /* set the saved program status register */
    stk--;
    *stk = (unsigned int) NEW_TASK_CPSR;

    return (char *)stk;
}

void thread_print_stack(void)
{
    register void *stack = 0;
    __asm__("mov %0, sp" : "=r"(stack));

    register unsigned int *s = (unsigned int *)stack;
    printf("task: %X SP: %X\n", (unsigned int) sched_active_thread, (unsigned int) stack);
    register int i = 0;
    s += 5;

    while (*s != STACK_MARKER) {
        printf("STACK (%d) addr=%X = %X \n", i, (unsigned int) s, (unsigned int) *s);
        s++;
        i++;
    }

    printf("STACK (%d)= %X \n", i, *s);
}

void *thread_isr_stack_start(void)
{
    extern uintptr_t __stack_irq_start;
    return (void *)&__stack_irq_start;
}

void *thread_isr_stack_pointer(void)
{
    void *_sp;

    /* If we are not in interrupt mode, the interrupt stack pointer will
     * always point to the start of the interrupt stack.
     */
    if (irq_is_in()) {
        /* read stack pointer */
        __asm volatile ("mov %0, r13" : "=r" (_sp) );
    } else {
        _sp = thread_isr_stack_start();
    }

    return _sp;
}

/* This function returns the number of bytes used on the ISR stack */
int thread_isr_stack_usage(void)
{
    uint32_t *ptr = (uint32_t*) &irq_stack[0];

    while(((*ptr) == STACK_CANARY_WORD) && (ptr < (uint32_t*) &irq_stack[ISR_STACKSIZE])) {
        ++ptr;
    }

    ptrdiff_t num_used_words = (uint32_t*) &irq_stack[ISR_STACKSIZE] - ptr;

    return num_used_words;
}

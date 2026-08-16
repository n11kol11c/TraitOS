; TrigerOS — context switch primitives (M6a preemptive scheduler)
;
; ttask_switch_ctx() saves the callee-saved registers + stack pointer of the
; current task into current_task->context, restores next_task->context, and
; resumes there. It must be called with interrupts disabled; the caller sets
; next_task first. Because only callee-saved registers are saved, the C ABI
; guarantees the interrupted task's other live registers are already on its
; stack.
;
; New tasks are entered through ttask_entry_iret: the initial stack built by
; ttask_init_stack() contains the saved callee-saved block, then a return
; address (ttask_entry_iret), then an iretq frame (rip = ttask_entry). The
; first switch into the task pops the block, `ret`s to ttask_entry_iret, and
; iretq starts the task with interrupts enabled on its own stack.

BITS 64

section .text

extern current_task
extern next_task

global ttask_switch_ctx
ttask_switch_ctx:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov rdi, [rel current_task]
    mov [rdi], rsp                 ; current_task->context = rsp
    mov rax, [rel next_task]
    mov [rel current_task], rax
    mov rsp, [rax]                 ; rsp = next_task->context
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

global ttask_entry_iret
ttask_entry_iret:
    iretq

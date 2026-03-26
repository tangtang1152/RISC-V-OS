.section .text
.globl kernel_entry
.globl user_entry

.balign 4
kernel_entry:
    j .

.balign 4
user_entry:
    ecall
    j .
.section .rodata
user_msg:
    .asciz "hello from user mode\n"
.section .text
.globl kernel_entry
.globl user_entry

kernel_entry:
    j .

user_entry:
    ecall
    j .
.section .rodata
user_msg:
    .asciz "hello from user mode\n"
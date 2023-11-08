    .global hello

    .section .data
msg:
    .ascii "Hello fixpoint!\n"
len = . - msg

    .section .text
hello:
    mov $1, %rax        # syscall number for sys_write
    mov $1, %rdi        # file descriptor 1 is stdout
    lea msg(%rip), %rsi      # address of string to output
    mov $len, %rdx      # number of bytes
    syscall             # invoke operating system to do the write

    # Exit the program
    mov $60, %rax       # syscall number for sys_exit
    xor %rdi, %rdi      # exit code 0
    syscall             # invoke operating system to exit


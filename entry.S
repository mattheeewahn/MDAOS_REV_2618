.code16gcc
.text
.global mystart
mystart:
    ljmp $0, $.setcs
.setcs:
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $0xD000, %sp
    cld
    call main
.hang:
    hlt
    jmp .hang

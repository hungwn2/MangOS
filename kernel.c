#include <stdint.h>


struct gdt{
    unsigned int address;
    unsigned short size;
}__attribute__((packed));

struct cpu_state {
unsigned int eax;
unsigned int ebx;
unsigned int ecx;
.
.
.
unsigned int esp;
} __attribute__((packed));

struct stack_state {
unsigned int error_code;
unsigned int eip;
unsigned int cs;
unsigned int eflags;
} __attribute__((packed));



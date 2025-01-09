#include "user.h"

void main(void) {
    printf("hello world from user space\n");
    *((unsigned int*) 0x80200000) = 0x123;
    for (;;);
}
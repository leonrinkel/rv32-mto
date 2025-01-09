#!/bin/bash

qemu-system-riscv32 \
    -machine virt \
    -bios default \
    -nographic \
    -serial mon:stdio \
    --no-reboot \
    -kernel build/kernel/kernel.elf
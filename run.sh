#!/bin/bash

qemu-system-riscv32 \
    -machine virt \
    -bios default \
    -device virtio-gpu-device,bus=virtio-mmio-bus.0 \
    -serial mon:stdio \
    --no-reboot \
    -kernel build/kernel/kernel.elf
CROSS = riscv64-unknown-elf-
CC = $(CROSS)gcc
OBJCOPY = $(CROSS)objcopy

CFLAGS = -Wall -Wextra -O0 -g -ffreestanding -nostdlib -nostartfiles
CFLAGS += -march=rv64gc -mabi=lp64 -mcmodel=medany
LDFLAGS = -T linker.ld -nostdlib

all: kernel.bin

kernel.elf: start.S kmain.c sbi.c sbi.h linker.ld trap.S trap.c riscv.h syscall.h user.c proc.c proc.h timer.h timer.c vm.h vm.c memlayout.h uaccess.c uaccess.h kalloc.c kalloc.h
	$(CC) $(CFLAGS) start.S kmain.c sbi.c trap.c trap.S user.c proc.c timer.c vm.c uaccess.c kalloc.c $(LDFLAGS) -o kernel.elf

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin

run: kernel.elf
	qemu-system-riscv64 \
		-machine virt \
		-nographic \
		-bios default \
		-kernel kernel.elf

# Run with timeout + log capture for automated testing
test: kernel.elf
	timeout 5 qemu-system-riscv64 \
		-machine virt \
		-nographic \
		-bios default \
		-kernel kernel.elf \
		-serial file:qemu.log || true; \
		echo "=== QEMU exited ==="

# Run and tee output to file (human + log)
run-log: kernel.elf
	qemu-system-riscv64 \
		-machine virt \
		-nographic \
		-bios default \
		-kernel kernel.elf 2>&1 | tee qemu.log

# Quick check: compile only, no run
check:
	$(CC) $(CFLAGS) start.S kmain.c sbi.c trap.c trap.S user.c proc.c timer.c vm.c uaccess.c kalloc.c $(LDFLAGS) -o kernel.elf
	@echo "=== check passed ==="

clean:
	rm -f kernel.elf kernel.bin qemu.log

CROSS = riscv64-unknown-elf-
CC = $(CROSS)gcc
OBJCOPY = $(CROSS)objcopy

CFLAGS = -Wall -Wextra -O0 -g -ffreestanding -nostdlib -nostartfiles
CFLAGS += -march=rv64gc -mabi=lp64 -mcmodel=medany
LDFLAGS = -T linker.ld -nostdlib

all: kernel.bin

kernel.elf: start.S main.c sbi.c sbi.h linker.ld trap.S trap.c riscv.h syscall.h user.c
	$(CC) $(CFLAGS) start.S main.c sbi.c trap.c trap.S user.c $(LDFLAGS) -o kernel.elf

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin

run: kernel.elf
	qemu-system-riscv64 \
		-machine virt \
		-nographic \
		-bios default \
		-kernel kernel.elf

clean:
	rm -f kernel.elf kernel.bin
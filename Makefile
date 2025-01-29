QEMU:=qemu-system-riscv32
OBJCOPY:=llvm-objcopy
CC:=clang
CFLAGS:=-std=c11 -O2 -g3 -Wall -Wextra --target=riscv32 -ffreestanding -nostdlib -I ./include/common/
KCFLAGS:=-I ./include/kernel/
UCFLAGS:=-I ./include/user/

DEPS:=$(wildcard build/*.d)

DISKFILES:=$(wildcard disk/*)

.PHONY: all run clean shell kernel disk

all:
	@echo ${DEPS}

run: build/kernel.elf build/disk.tar
	${QEMU} -machine virt -smp 4 -bios default -nographic -serial mon:stdio --no-reboot \
		-d unimp,guest_errors,int,cpu_reset -D qemu.log \
		-drive id=drive0,file=build/disk.tar,format=raw,if=none \
		-device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
		-kernel kernel.elf

clean:
	rm qemu.log disk.tar shell.bin.o shell.bin shell.elf shell.map kernel.elf kernel.map

shell: build/shell.bin.o

kernel: build/kernel.elf

disk: build/disk.tar

build/shell.elf build/shell.map: shell.c user.c common.c user.ld
	mkdir -p $(@D)
	${CC} ${CFLAGS} ${UCFLAGS} -Wl,-Tuser.ld -Wl,-Map=build/shell.map -MD -o build/shell.elf shell.c user.c common.c

build/shell.bin: build/shell.elf
	${OBJCOPY} --set-section-flags .bss=alloc,contents -O binary $^ $@

build/shell.bin.o: build/shell.bin
	${OBJCOPY} -Ibinary -Oelf32-littleriscv $^ $@

build/kernel.elf build/kernel.map: kernel.c common.c build/shell.bin.o
	${CC} ${CFLAGS} ${KCFLAGS} -Wl,-Tkernel.ld -Wl,-Map=build/kernel.map -MD -o build/kernel.elf $^

build/disk.tar: ${DISKFILES}
	tar -cf $@ --format=ustar -C disk $(patsubst disk/%,%,$^)

BUILD_DIR:=./build

QEMU:=qemu-system-riscv32
OBJCOPY:=llvm-objcopy

CC:=clang
CFLAGS:=-std=c11 -O2 -g3 -Wall -Wextra --target=riscv32 -ffreestanding -nostdlib -I ./include/common/
KCFLAGS:=-I ./include/kernel/
UCFLAGS:=-I ./include/user/

rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2)$(filter $(subst *,%,$2),$d))

KSRC:=$(call rwildcard,src/kernel,*.c)
USRC:=$(call rwildcard,src/user,*.c)
CSRC:=$(call rwildcard,src/common,*.c)

KOBJ:=$(KSRC:src/%.c=${BUILD_DIR}/%.o)
UOBJ:=$(USRC:src/%.c=${BUILD_DIR}/%.o)
COBJ:=$(CSRC:src/%.c=${BUILD_DIR}/%.o)

DEPS:=$(call rwildcard,build,*.d)

DISKFILES:=$(wildcard disk/*)

.PHONY: all run run-quiet clean shell kernel disk
.INTERMEDIATE: ${BUILD_DIR}/shell.bin

all: shell kernel disk

run: ${BUILD_DIR}/kernel.elf ${BUILD_DIR}/disk.tar
	${QEMU} -machine virt -smp 4 -bios default -nographic -serial mon:stdio --no-reboot \
		-d unimp,guest_errors,int,cpu_reset -D qemu.log \
		-drive id=drive0,file=${BUILD_DIR}/disk.tar,format=raw,if=none \
		-device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
		-kernel ${BUILD_DIR}/kernel.elf -append "verbose"

run-quiet: ${BUILD_DIR}/kernel.elf ${BUILD_DIR}/disk.tar
	${QEMU} -machine virt -smp 4 -bios default -nographic -serial mon:stdio --no-reboot \
		-d unimp,guest_errors,int,cpu_reset -D qemu.log \
		-drive id=drive0,file=${BUILD_DIR}/disk.tar,format=raw,if=none \
		-device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
		-kernel ${BUILD_DIR}/kernel.elf

clean:
	@rm -vrf ${BUILD_DIR}/ qemu.log

shell: ${BUILD_DIR}/shell.bin.o

kernel: ${BUILD_DIR}/kernel.elf

disk: ${BUILD_DIR}/disk.tar

include ${DEPS}

${BUILD_DIR}/kernel/%.o : src/kernel/%.c
	@mkdir -p $(@D)
	${CC} ${CFLAGS} ${KCFLAGS} -MD -c $< -o $@

${BUILD_DIR}/common/%.o : src/common/%.c
	@mkdir -p $(@D)
	${CC} ${CFLAGS} -MD -c $< -o $@

${BUILD_DIR}/user/%.o : src/user/%.c
	@mkdir -p $(@D)
	${CC} ${CFLAGS} ${UCFLAGS} -MD -c $< -o $@

${BUILD_DIR}/shell.elf ${BUILD_DIR}/shell.map &: ${BUILD_DIR}/user/shell.o ${BUILD_DIR}/user/user.o ${BUILD_DIR}/common/common.o user.ld
	${CC} ${CFLAGS} ${UCFLAGS} -Wl,-Map=${BUILD_DIR}/shell.map -o $@ $^

${BUILD_DIR}/shell.bin.o ${BUILD_DIR}/shell.bin &: ${BUILD_DIR}/shell.elf
	${OBJCOPY} --set-section-flags .bss=alloc,contents -O binary $^ ${BUILD_DIR}/shell.bin
	${OBJCOPY} -Ibinary -Oelf32-littleriscv ${BUILD_DIR}/shell.bin $@

${BUILD_DIR}/kernel.elf ${BUILD_DIR}/kernel.map &: ${BUILD_DIR}/kernel/kernel.o ${BUILD_DIR}/common/common.o ${BUILD_DIR}/kernel/sbi/sbi.o ${BUILD_DIR}/kernel/devices/device_tree.o ${BUILD_DIR}/shell.bin.o kernel.ld
	${CC} ${CFLAGS} ${KCFLAGS} -Wl,-Map=${BUILD_DIR}/kernel.map -o $@ $^

${BUILD_DIR}/disk.tar: ${DISKFILES}
	tar -cf $@ --format=ustar -C disk $(patsubst disk/%,%,$^)

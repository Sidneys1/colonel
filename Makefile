BUILD_DIR:=./build
CORES:=2

QEMU:=qemu-system-riscv32
OBJCOPY:=llvm-objcopy
GDB:=gdb-multiarch

CC:=bear --append --output compile_commands.json -- clang
CFLAGSEXTRA?=-DDEBUG
CFLAGS:=-std=c23 -O0 -ggdb -Wall -Wextra --target=riscv32 -ffreestanding -nostdlib -I ./include/common/ ${CFLAGSEXTRA}
KCFLAGS:=-isystem ./include/kernel/
UCFLAGS:=-isystem ./include/user/

rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2)$(filter $(subst *,%,$2),$d))
GUARD = ${BUILD_DIR}/$(1)_GUARD_$(shell echo $($(1)) | md5sum | cut -d ' ' -f 1)

KSRC:=$(call rwildcard,src/kernel,*.c)
USRC:=$(call rwildcard,src/user,*.c)
CSRC:=$(call rwildcard,src/common,*.c)

KOBJ:=$(KSRC:src/%.c=${BUILD_DIR}/%.o)
UOBJ:=$(USRC:src/%.c=${BUILD_DIR}/%.o)
COBJ:=$(CSRC:src/%.c=${BUILD_DIR}/%.o)

DEPS:=$(call rwildcard,build,*.d)

DISKFILES:=$(wildcard disk/*)

.PHONY: all run run-quiet debug test tidy clean shell kernel disk
.INTERMEDIATE: ${BUILD_DIR}/shell.bin
.NOTPARALLEL: test

all: shell kernel disk

run: ${BUILD_DIR}/kernel.elf ${BUILD_DIR}/disk.tar
	${QEMU} -machine virt -smp ${CORES} -bios default -nographic -serial mon:stdio --no-reboot \
		-d unimp,guest_errors,int,cpu_reset -D qemu.log \
		-drive id=drive0,file=${BUILD_DIR}/disk.tar,format=raw,if=none \
		-device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
		-kernel ${BUILD_DIR}/kernel.elf -append "verbose"

run-quiet: ${BUILD_DIR}/kernel.elf ${BUILD_DIR}/disk.tar
	${QEMU} -machine virt -smp ${CORES} -bios default -nographic -serial mon:stdio --no-reboot \
		-d unimp,guest_errors,int,cpu_reset -D qemu.log \
		-drive id=drive0,file=${BUILD_DIR}/disk.tar,format=raw,if=none \
		-device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
		-kernel ${BUILD_DIR}/kernel.elf

debug: ${BUILD_DIR}/kernel.elf ${BUILD_DIR}/disk.tar
	${QEMU} -machine virt -smp ${CORES} -bios default -nographic -serial mon:stdio --no-reboot \
		-d unimp,guest_errors,int,cpu_reset -D qemu.log \
		-drive id=drive0,file=${BUILD_DIR}/disk.tar,format=raw,if=none \
		-device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
		-kernel ${BUILD_DIR}/kernel.elf -s -S \
	& ${GDB} -tui -q -ex "file ${BUILD_DIR}/kernel.elf" \
		-ex "target remote 127.0.0.1:1234" \
		-ex "b boot" \
		-ex "b kernel_main" \
		-ex "b secondary_boot" \
		-ex "c"

test:
	${MAKE} CFLAGSEXTRA="${CFLAGSEXTRA} -DTESTS" run

tidy:
	clang-tidy -system-headers -header-filter=".*" -p ${BUILD_DIR} ${KSRC} ${CSRC} ${USRC}

clean:
	@rm -vrf ${BUILD_DIR}/ qemu.log compile_commands.json

shell: ${BUILD_DIR}/shell.bin.o

kernel: ${BUILD_DIR}/kernel.elf

disk: ${BUILD_DIR}/disk.tar

include ${DEPS}

$(call GUARD,CFLAGSEXTRA):
	rm -f build/CFLAGSEXTRA_GUARD_*
	@mkdir -p $(@D)
	touch $@

${BUILD_DIR}/kernel/%.o : src/kernel/%.c $(call GUARD,CFLAGSEXTRA)
	@mkdir -p $(@D)
	${CC} ${CFLAGS} ${KCFLAGS} -MD -c $< -o $@

${BUILD_DIR}/common/%.o : src/common/%.c $(call GUARD,CFLAGSEXTRA)
	@mkdir -p $(@D)
	${CC} ${CFLAGS} -MD -c $< -o $@

${BUILD_DIR}/user/%.o : src/user/%.c $(call GUARD,CFLAGSEXTRA)
	@mkdir -p $(@D)
	${CC} ${CFLAGS} ${UCFLAGS} -MD -c $< -o $@

${BUILD_DIR}/shell.elf ${BUILD_DIR}/shell.map &: ${UOBJ} ${COBJ} user.ld
	@echo Deps: $^
	${CC} ${CFLAGS} ${UCFLAGS} -Wl,-Map=${BUILD_DIR}/shell.map -o $@ $^

${BUILD_DIR}/shell.bin.o ${BUILD_DIR}/shell.bin &: ${BUILD_DIR}/shell.elf
	${OBJCOPY} --set-section-flags .bss=alloc,contents -O binary $^ ${BUILD_DIR}/shell.bin
	${OBJCOPY} -Ibinary -Oelf32-littleriscv ${BUILD_DIR}/shell.bin $@

# ${BUILD_DIR}/kernel.elf ${BUILD_DIR}/kernel.map: $(call GUARD,CFLAGSEXTRA)
${BUILD_DIR}/kernel.elf ${BUILD_DIR}/kernel.map &: ${KOBJ} ${COBJ} ${BUILD_DIR}/shell.bin.o kernel.ld
	${CC} ${CFLAGS} ${KCFLAGS} -Wl,-Map=${BUILD_DIR}/kernel.map -o $@ $^

${BUILD_DIR}/disk.tar: ${DISKFILES}
	tar -cf $@ --format=ustar -C disk $(patsubst disk/%,%,$^)

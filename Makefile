# Build output directory
BUILD_DIR:=./build

# Number of cores for QEMU
CORES:=2
# QEMU executable
QEMU:=qemu-system-riscv32
# objcopy executable
OBJCOPY:=llvm-objcopy
# GDB executable
GDB:=gdb-multiarch

# C compiler. We tack on `bear` to get `compile_commands.json`
CC:=bear --append --output compile_commands.json -- clang
# "extra" CFLAGS. By default, we build in DEBUG mode.
CFLAGSEXTRA?=-DDEBUG -O0 -ggdb
# Cflags. Appends CFLAGSEXTRA.
CFLAGS=-std=c23 -Wall -Wextra --target=riscv32 -march=rv32gc -mabi=ilp32f -ffreestanding -nostdlib -isystem ./include/stdlib -isystem ./include/common/ ${CFLAGSEXTRA}
# Extra kernel-mode flags.
KCFLAGS:=-isystem ./include/kernel/
# Extra user-mode flags.
UCFLAGS:=-isystem ./include/user/

# Recursive wildcard globs
rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2)$(filter $(subst *,%,$2),$d))
# Creates guards for variables (lets targets depend on variable contents). See target(s) `$(call guard,XXX)`.
guard = ${BUILD_DIR}/$(1)_GUARD_$(shell echo $($(1)) | md5sum | cut -d ' ' -f 1)

# Kernel source files
KERNEL_SRC:=$(call rwildcard,src/kernel,*.c)
# Userland source files.
USER_SRC:=$(call rwildcard,src/user,*.c)
# Common souce files.
COMMON_SRC:=$(call rwildcard,src/common,*.c)
# Stdlib source files.
STDLIB_SRC:=$(call rwildcard,src/stdlib,*.c)

# Kernel object files.
KERNEL_OBJ:=$(KERNEL_SRC:src/%.c=${BUILD_DIR}/%.o)
# Userland object files.
USER_OBJ:=$(USER_SRC:src/%.c=${BUILD_DIR}/%.o)
# Common object files.
COMMON_OBJ:=$(COMMON_SRC:src/%.c=${BUILD_DIR}/%.o)
# stdlib object files.
STDLIB_OBJ:=$(STDLIB_SRC:src/%.c=${BUILD_DIR}/%.o)

# Makefile dependencies for include - see `-MD` param to build object files.
DEPS:=$(call rwildcard,build,*.d)

# Files needed for building disk image (tar).
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
	clang-tidy -system-headers -header-filter=".*" -p ${BUILD_DIR} ${KERNEL_SRC} ${COMMON_SRC} ${USER_SRC} ${STDLIB_SRC}

clean:
	@rm -vrf ${BUILD_DIR}/ qemu.log compile_commands.json

shell: disk/shell.bin

kernel: ${BUILD_DIR}/kernel.elf

disk: ${BUILD_DIR}/disk.tar

include ${DEPS}

$(call guard,CFLAGSEXTRA):
	rm -f build/CFLAGSEXTRA_GUARD_*
	@mkdir -p $(@D)
	touch $@

${BUILD_DIR}/kernel/%.o : src/kernel/%.c $(call guard,CFLAGSEXTRA)
	@mkdir -p $(@D)
	${CC} ${CFLAGS} ${KCFLAGS} -MD -c $< -o $@

${BUILD_DIR}/common/%.o : src/common/%.c $(call guard,CFLAGSEXTRA)
	@mkdir -p $(@D)
	${CC} ${CFLAGS} -MD -c $< -o $@

${BUILD_DIR}/stdlib/%.o : src/stdlib/%.c $(call guard,CFLAGSEXTRA)
	@mkdir -p $(@D)
	${CC} ${CFLAGS} -MD -c $< -o $@

${BUILD_DIR}/user/%.o : src/user/%.c $(call guard,CFLAGSEXTRA)
	@mkdir -p $(@D)
	${CC} ${CFLAGS} ${UCFLAGS} -MD -c $< -o $@

${BUILD_DIR}/shell.elf ${BUILD_DIR}/shell.map &: ${USER_OBJ} ${COMMON_OBJ} ${STDLIB_OBJ} user.ld
	${CC} ${CFLAGS} ${UCFLAGS} -Wl,-Map=${BUILD_DIR}/shell.map -o $@ $^

${BUILD_DIR}/shell.stripped.elf: ${BUILD_DIR}/shell.elf
	llvm-strip -UR.comment -so $@ $^

disk/shell.bin: ${BUILD_DIR}/shell.stripped.elf
	${OBJCOPY} --set-section-flags .bss=alloc,contents -O binary $^ disk/shell.bin

${BUILD_DIR}/kernel.elf: $(call guard,CFLAGSEXTRA)
${BUILD_DIR}/kernel.elf ${BUILD_DIR}/kernel.map &: ${KERNEL_OBJ} ${COMMON_OBJ} ${STDLIB_OBJ} kernel.ld  
	${CC} ${CFLAGS} ${KCFLAGS} -Wl,-Map=${BUILD_DIR}/kernel.map -o $@ $^

${BUILD_DIR}/disk.tar: ${DISKFILES} disk/shell.bin
	tar -cf $@ --format=ustar -C disk $(patsubst disk/%,%,$^)

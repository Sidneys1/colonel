# Build output directory
BUILD_DIR:=./build
# Number of cores for QEMU
CORES?=2
# QEMU memory
MEM:=128M
# QEMU output
LOG:=qemu.log
# QEMU executable
QEMU:=qemu-system-riscv32

# Kernel flags
QAPPEND=verbose

# QEMU options
QFLAGS=-machine virt -bios default --no-reboot \
        -d unimp,guest_errors,int,cpu_reset -D ${LOG} \
        -m ${MEM} -smp ${CORES} -serial mon:stdio \
        -drive id=drive0,file=${BUILD_DIR}/disk.tar,format=raw,if=none \
        -device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
        -device VGA,romfile=/usr/share/vgabios/vgabios-stdvga.bin \
        -kernel ${BUILD_DIR}/kernel.elf -append ${QAPPEND}

# objcopy executable
OBJCOPY:=llvm-objcopy
# GDB executable
GDB:=gdb-multiarch

# C compiler. We tack on `bear` to get `compile_commands.json`
CC:=clang
CPP:=clang++
# "extra" CFLAGS. By default, we build in DEBUG mode.
CFLAGSEXTRA?=-DDEBUG -O0 -ggdb -fno-omit-frame-pointer
# Linker flags
LDFLAGS?=-fuse-ld=lld -Wl,--undefined=main -Wl,--undefined=exit -Wl,--undefined=kernel_main
# Cflags. Appends CFLAGSEXTRA.  -mabi=ilp32f -flto
CFLAGS=-std=c23 -Wall -Wextra -Wno-string-plus-int --target=riscv32 -march=rv32g -ffreestanding -nostdlib -isystem ./include/stdlib -isystem ./include/common/ ${CFLAGSEXTRA}
# Extra kernel-mode flags.
KCFLAGS:=-isystem ./include/kernel/
# Extra user-mode flags.
UCFLAGS:=-isystem ./include/user/

# -flto -Wl,--gc-sections,--print-gc-sections  -ffunction-sections -fdata-sections
CPPLDFLAGS?=-fuse-ld=lld
CPPFLAGSEXTRA?=-DDEBUG -O0 -ggdb -fno-omit-frame-pointer
CPPFLAGS=-std=c++23 -Wall -Wextra -Wno-string-plus-int --target=riscv32 -march=rv32g -ffreestanding -nostdlib -isystem ./include/stdlib -isystem ./include/common/ ${CPPFLAGSEXTRA}
UCPPFLAGS:=-isystem ./include/user/

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

USER_SRC_CPP:=$(call rwildcard,src/user,*.cpp)

# Kernel object files.
KERNEL_OBJ:=$(KERNEL_SRC:src/%.c=${BUILD_DIR}/%.o)
# Userland object files.
USER_OBJ:=$(USER_SRC:src/%.c=${BUILD_DIR}/%.o)
# Common object files.
COMMON_OBJ:=$(COMMON_SRC:src/%.c=${BUILD_DIR}/%.o)
# stdlib object files.
STDLIB_OBJ:=$(STDLIB_SRC:src/%.c=${BUILD_DIR}/%.o)

USER_OBJ_CPP:=$(USER_SRC_CPP:src/%.cpp=${BUILD_DIR}/%.cpp.o)

# Makefile dependencies for include - see `-MD` param to build object files.
DEPS:=$(call rwildcard,build,*.d)

# Files needed for building disk image (tar).
DISKFILES:=$(wildcard disk/*)

.PHONY: all run run-quiet debug test tidy format clean shell kernel disk graph
.INTERMEDIATE: ${BUILD_DIR}/shell.bin
.NOTPARALLEL: test

all: shell kernel disk

run-noinit: QAPPEND=noinit

run-quiet: QAPPEND=""

run-quiet: run
run-noinit: run

run: ${BUILD_DIR}/kernel.elf ${BUILD_DIR}/disk.tar
	${QEMU} ${QFLAGS}

debug: ${BUILD_DIR}/kernel.elf ${BUILD_DIR}/disk.tar
	${QEMU} ${QFLAGS} -s -S \
	& ${GDB} -tui -q -ex "file ${BUILD_DIR}/kernel.elf" \
		-ex "target remote 127.0.0.1:1234" \
		-ex "b boot" \
		-ex "c"

debug-tmux: ${BUILD_DIR}/kernel.elf ${BUILD_DIR}/disk.tar
	@if [ -z "$$TMUX" ]; then echo "Not running under tmux!" 1>&2 && exit 1; fi
	pane1=$$(tmux split-window -hPF "#{pane_id}" -l '30%' -d 'tail -f /dev/null') \
	&& tty1=$$(tmux display-message -p -t "$$pane1" '#{pane_tty}') \
	&& pane2=$$(tmux split-window -P -F "#{pane_id}" -l '25%' -d '${QEMU} ${QFLAGS} -s -S & tail -F qemu.out') \
	&& tty2=$$(tmux display-message -p -t "$$pane2" '#{pane_tty}') \
	&& ${GDB} -tui -q -ex "file ${BUILD_DIR}/kernel.elf" \
		-ex "target remote 127.0.0.1:1234" \
		-ex "layout split" \
		-ex "dashboard -output $$tty1" \
		-ex "b kernel_main" \
		-ex "run > $$tty2" \
	; echo "Killing panes..." && tmux kill-pane -t $$pane2 & tmux kill-pane -t $$pane1

test:
	${MAKE} CFLAGSEXTRA="${CFLAGSEXTRA} -DTESTS" run

tidy:
	clang-tidy -system-headers -header-filter=".*" -p ${BUILD_DIR} ${KERNEL_SRC} ${COMMON_SRC} ${USER_SRC} ${STDLIB_SRC}

format:
	clang-format -i $$(find src/ include/ -name '*.h' -o -name '*.c')

clean:
	@rm -vrf ${BUILD_DIR}/ qemu.log compile_commands.json disk/shell.bin

shell: disk/shell.cpp.elf

kernel: ${BUILD_DIR}/kernel.elf

disk: ${BUILD_DIR}/disk.tar

graph:
	make -dn MAKE=: all | sed -rn "s/^(\s+)Considering target file '(.*)'\.$$/\1\2/p"

include ${DEPS}

$(call guard,CFLAGSEXTRA):
	rm -f build/CFLAGSEXTRA_GUARD_*
	@mkdir -p $(@D)
	touch $@

compile_commands.json:
	bear --output compile_commands.json -- ${MAKE} all

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

${BUILD_DIR}/user/%.cpp.o : src/user/%.cpp
	@mkdir -p $(@D)
	${CPP} ${CPPFLAGS} ${UCPPFLAGS} -MD -c $< -o $@

${BUILD_DIR}/stdlib.a : ${STDLIB_OBJ}
	ar rcs $@ $^

${BUILD_DIR}/shell.cpp.elf ${BUILD_DIR}/shell.cpp.map &: ${BUILD_DIR}/user/user.o ${BUILD_DIR}/user/shell.cpp.o ${COMMON_OBJ} ${BUILD_DIR}/stdlib.a user.ld
	${CPP} ${CPPFLAGS} ${UCPPFLAGS} ${LDFLAGS} -Wl,-Map=${BUILD_DIR}/shell.cpp.map -o $@ $^

${BUILD_DIR}/shell.elf ${BUILD_DIR}/shell.map &: ${USER_OBJ} ${COMMON_OBJ} ${BUILD_DIR}/stdlib.a user.ld
	${CC} ${CFLAGS} ${UCFLAGS} ${LDFLAGS} -Wl,-Map=${BUILD_DIR}/shell.map -o $@ $^

${BUILD_DIR}/%.elf ${BUILD_DIR}/%.map &: ${BUILD_DIR}/user/user.o ${BUILD_DIR}/user/%.cpp.o ${COMMON_OBJ} ${BUILD_DIR}/stdlib.a user.ld
	${CPP} ${CPPFLAGS} ${UCPPFLAGS} ${CPPLDFLAGS} ${LDFLAGS} -Wl,-Map=${BUILD_DIR}/init.map -o $@ $^

${BUILD_DIR}/%.stripped.elf: ${BUILD_DIR}/%.elf
	llvm-strip -UR.comment -so $@ $^

disk/%.elf: ${BUILD_DIR}/%.stripped.elf
	cp $^ $@

disk/%.bin: ${BUILD_DIR}/%.stripped.elf
	${OBJCOPY} --set-section-flags .bss=alloc,contents -O binary $^ $@

${BUILD_DIR}/kernel.elf: $(call guard,CFLAGSEXTRA)
${BUILD_DIR}/kernel.elf ${BUILD_DIR}/kernel.map &: ${KERNEL_OBJ} ${COMMON_OBJ} ${BUILD_DIR}/stdlib.a kernel.ld
	${CC} ${CFLAGS} ${KCFLAGS} ${LDFLAGS} -Wl,-Map=${BUILD_DIR}/kernel.map -o $@ $^

${BUILD_DIR}/disk.tar: ${DISKFILES} disk/shell.cpp.elf disk/init.elf
	tar -cf $@ --format=ustar -C disk $(patsubst disk/%,%,$^)

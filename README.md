# Colonel

A 32-bit RISC-V kernel/operating system.

<details><summary><b>Get Started</b></summary>

```sh
## Install dependencies (clang/LLVM, QEMU, GDB, OpenSBI)
sudo apt install -y clang llvm lld qemu-system-riscv32 gdb-multiarch
wget https://github.com/qemu/qemu/raw/v8.0.4/pc-bios/opensbi-riscv32-generic-fw_dynamic.bin

## Build and run
make run
```

</details>

**Colonel currently has:**
- The beginnings of a `libc` implementation (see 📁`include/libc/`).
- The basics of multicore execution.
- Kernel memory management (page allocation and slab allocators).
- Virtio block device support (can read TAR-formatted disks).
- Basic FDT (flattened device tree) support.
- Virtual-memory managed userspace with syscalls, context switching, and cooperative multitasking.
    - Launches `shell.bin` from disk.
- In development:
    - PCI devices.
    - VGA (over PCI).
    - Preemptive multitasking.

## Other Make Targets

```sh
# Run without the kernel `verbose` parameter
make run-quiet

# Run under GDB
make debug

# Format code and run clang-tidy
make format tidy

# Run tests (this is a work in progress)
make test
```

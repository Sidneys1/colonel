#include "kernel.h"
#include "common.h"

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef uint32_t size_t;

struct virtio_virtq *blk_request_vq;
struct virtio_blk_req *blk_req;
paddr_t blk_req_paddr;
unsigned blk_capacity;

extern char __bss[], __bss_end[], __stack_top[], __free_ram[], __free_ram_end[], __kernel_base[], _binary_shell_bin_start[], _binary_shell_bin_size[];

struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4,
                       long arg5, long fid, long eid) {
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a3 __asm__("a3") = arg3;
    register long a4 __asm__("a4") = arg4;
    register long a5 __asm__("a5") = arg5;
    register long a6 __asm__("a6") = fid;
    register long a7 __asm__("a7") = eid;

    __asm__ __volatile__("ecall"
                         : "=r"(a0), "=r"(a1)
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5),
                           "r"(a6), "r"(a7)
                         : "memory");
    return (struct sbiret){.error = a0, .value = a1};
}

uint32_t virtio_reg_read32(unsigned offset) {
    return *((volatile uint32_t *) (VIRTIO_BLK_PADDR + offset));
}

uint64_t virtio_reg_read64(unsigned offset) {
    return *((volatile uint64_t *) (VIRTIO_BLK_PADDR + offset));
}

void virtio_reg_write32(unsigned offset, uint32_t value) {
    *((volatile uint32_t *) (VIRTIO_BLK_PADDR + offset)) = value;
}

void virtio_reg_fetch_and_or32(unsigned offset, uint32_t value) {
    virtio_reg_write32(offset, virtio_reg_read32(offset) | value);
}

long getchar(void) {
    struct sbiret ret = sbi_call(0, 0, 0, 0, 0, 0, 0, 2);
    return ret.error;
}

void putchar(char ch) {
    sbi_call(ch, 0, 0, 0, 0, 0, 0, 1 /* Console Putchar */);
}

__attribute__((naked)) void switch_context(uint32_t *prev_sp,
                                           uint32_t *next_sp) {
    __asm__ __volatile__(
        "addi sp, sp, -13 * 4\n" // Allocate stack space for 13 4-byte registers
        "sw ra,  0  * 4(sp)\n"   // Save callee-saved registers only
        "sw s0,  1  * 4(sp)\n"
        "sw s1,  2  * 4(sp)\n"
        "sw s2,  3  * 4(sp)\n"
        "sw s3,  4  * 4(sp)\n"
        "sw s4,  5  * 4(sp)\n"
        "sw s5,  6  * 4(sp)\n"
        "sw s6,  7  * 4(sp)\n"
        "sw s7,  8  * 4(sp)\n"
        "sw s8,  9  * 4(sp)\n"
        "sw s9,  10 * 4(sp)\n"
        "sw s10, 11 * 4(sp)\n"
        "sw s11, 12 * 4(sp)\n"
        "sw sp, (a0)\n"         // *prev_sp = sp;
        "lw sp, (a1)\n"         // Switch stack pointer (sp) here
        "lw ra,  0  * 4(sp)\n"  // Restore callee-saved registers only
        "lw s0,  1  * 4(sp)\n"
        "lw s1,  2  * 4(sp)\n"
        "lw s2,  3  * 4(sp)\n"
        "lw s3,  4  * 4(sp)\n"
        "lw s4,  5  * 4(sp)\n"
        "lw s5,  6  * 4(sp)\n"
        "lw s6,  7  * 4(sp)\n"
        "lw s7,  8  * 4(sp)\n"
        "lw s8,  9  * 4(sp)\n"
        "lw s9,  10 * 4(sp)\n"
        "lw s10, 11 * 4(sp)\n"
        "lw s11, 12 * 4(sp)\n"
        "addi sp, sp, 13 * 4\n"  // We've popped 13 4-byte registers from the stack
        "ret\n"
    );
}

paddr_t alloc_pages(uint32_t n) {
    static paddr_t next_paddr = (paddr_t) __free_ram;
    paddr_t paddr = next_paddr;
    next_paddr += n * PAGE_SIZE;

    if (next_paddr > (paddr_t) __free_ram_end)
        PANIC("out of memory");

    memset((void *) paddr, 0, n * PAGE_SIZE);
    return paddr;
}

void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags) {
    if (!is_aligned(vaddr, PAGE_SIZE))
        PANIC("unaligned vaddr %x", vaddr);

    if (!is_aligned(paddr, PAGE_SIZE))
        PANIC("unaligned paddr %x", paddr);

    uint32_t vpn1 = (vaddr >> 22) & 0x3ff;
    if ((table1[vpn1] & PAGE_V) == 0) {
        // Create the non-existent 2nd level page table.
        uint32_t pt_paddr = alloc_pages(1);
        table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
    }

    // Set the 2nd level page table entry to map the physical page.
    uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
    uint32_t *table0 = (uint32_t *) ((table1[vpn1] >> 10) * PAGE_SIZE);
    table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
}

struct virtio_virtq *virtq_init(unsigned index) {
    // Allocate a region for the virtqueue.
    paddr_t virtq_paddr = alloc_pages(align_up(sizeof(struct virtio_virtq), PAGE_SIZE) / PAGE_SIZE);
    struct virtio_virtq *vq = (struct virtio_virtq *) virtq_paddr;
    vq->queue_index = index;
    vq->used_index = (volatile uint16_t *) &vq->used.index;
    // 1. Select the queue writing its index (first queue is 0) to QueueSel.
    virtio_reg_write32(VIRTIO_REG_QUEUE_SEL, index);
    // 5. Notify the device about the queue size by writing the size to QueueNum.
    virtio_reg_write32(VIRTIO_REG_QUEUE_NUM, VIRTQ_ENTRY_NUM);
    // 6. Notify the device about the used alignment by writing its value in bytes to QueueAlign.
    virtio_reg_write32(VIRTIO_REG_QUEUE_ALIGN, 0);
    // 7. Write the physical number of the first page of the queue to the QueuePFN register.
    virtio_reg_write32(VIRTIO_REG_QUEUE_PFN, virtq_paddr);
    return vq;
}

void virtio_blk_init(void) {
    if (virtio_reg_read32(VIRTIO_REG_MAGIC) != 0x74726976)
        PANIC("virtio: invalid magic value");
    if (virtio_reg_read32(VIRTIO_REG_VERSION) != 1)
        PANIC("virtio: invalid version");
    if (virtio_reg_read32(VIRTIO_REG_DEVICE_ID) != VIRTIO_DEVICE_BLK)
        PANIC("virtio: invalid device id");

    // 1. Reset the device.
    virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, 0);
    // 2. Set the ACKNOWLEDGE status bit: the guest OS has noticed the device.
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_ACK);
    // 3. Set the DRIVER status bit.
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER);
    // 5. Set the FEATURES_OK status bit.
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_FEAT_OK);
    // 7. Perform device-specific setup, including discovery of virtqueues for the device
    blk_request_vq = virtq_init(0);
    // 8. Set the DRIVER_OK status bit.
    virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER_OK);

    // Get the disk capacity.
    blk_capacity = virtio_reg_read64(VIRTIO_REG_DEVICE_CONFIG + 0) * SECTOR_SIZE;
    kprintf("virtio-blk: capacity is %d bytes\n", blk_capacity);

    // Allocate a region to store requests to the device.
    blk_req_paddr = alloc_pages(align_up(sizeof(*blk_req), PAGE_SIZE) / PAGE_SIZE);
    blk_req = (struct virtio_blk_req *) blk_req_paddr;
}

struct process procs[PROCS_MAX]; // All process control structures.

__attribute__((naked)) void user_entry(void) {
    __asm__ __volatile__(
        "csrw sepc, %[sepc]        \n"
        "csrw sstatus, %[sstatus]  \n"
        "sret                      \n"
        :
        : [sepc] "r" (USER_BASE),
          [sstatus] "r" (SSTATUS_SPIE | SSTATUS_SUM)
    );
}

struct process *create_process(const void *image, size_t image_size) {
    // Find an unused process control structure.
    struct process *proc = NULL;
    int i;
    for (i = 0; i < PROCS_MAX; i++) {
        if (procs[i].state == PROC_UNUSED) {
            proc = &procs[i];
            break;
        }
    }

    if (!proc)
        PANIC("no free process slots");

    // Stack callee-saved registers. These register values will be restored in
    // the first context switch in switch_context.
    uint32_t *sp = (uint32_t *) &proc->stack[sizeof(proc->stack)];
    *--sp = 0;                      // s11
    *--sp = 0;                      // s10
    *--sp = 0;                      // s9
    *--sp = 0;                      // s8
    *--sp = 0;                      // s7
    *--sp = 0;                      // s6
    *--sp = 0;                      // s5
    *--sp = 0;                      // s4
    *--sp = 0;                      // s3
    *--sp = 0;                      // s2
    *--sp = 0;                      // s1
    *--sp = 0;                      // s0
    *--sp = (uint32_t) user_entry;  // ra

    uint32_t *page_table = (uint32_t *) alloc_pages(1);
    for (paddr_t paddr = (paddr_t) __kernel_base;
         paddr < (paddr_t) __free_ram_end; paddr += PAGE_SIZE)
        map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);

    // Map virtio
    map_page(page_table, VIRTIO_BLK_PADDR, VIRTIO_BLK_PADDR, PAGE_R | PAGE_W);

    // Map user pages.
    for (uint32_t off = 0; off < image_size; off += PAGE_SIZE) {
        paddr_t page = alloc_pages(1);

        // Consider the case where the data to be copied is smaller than the
        // page size.
        size_t remaining = image_size - off;
        size_t copy_size = PAGE_SIZE <= remaining ? PAGE_SIZE : remaining;

        // Fill and map the page.
        memcpy((void *) page, image + off, copy_size);
        map_page(page_table, USER_BASE + off, page,
                 PAGE_U | PAGE_R | PAGE_W | PAGE_X);
    }

    // Initialize fields.
    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->sp = (uint32_t) sp;
    proc->page_table = page_table;
    return proc;
}

struct process *current_proc; // Currently running process
struct process *idle_proc;    // Idle process

void yield(void) {
    // Search for a runnable process
    struct process *next = idle_proc;
    for (int i = 0; i < PROCS_MAX; i++) {
        struct process *proc = &procs[(current_proc->pid + i) % PROCS_MAX];
        if (proc->state == PROC_RUNNABLE && proc->pid > 0) {
            next = proc;
            break;
        }
    }

    // If there's no runnable process other than the current one, return and continue processing
    if (next == current_proc)
        return;

    __asm__ __volatile__(
        "csrw sscratch, %[sscratch]\n"
        :
        : [sscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
    );

    // Context switch
    struct process *prev = current_proc;
    current_proc = next;

    // Switch page table.
    __asm__ __volatile__(
        "sfence.vma\n"
        "csrw satp, %[satp]\n"
        "sfence.vma\n"
        "csrw sscratch, %[sscratch]\n"
        :
        // Don't forget the trailing comma!
        : [satp] "r" (SATP_SV32 | ((uint32_t) next->page_table / PAGE_SIZE)),
          [sscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
    );

    switch_context(&prev->sp, &next->sp);
}

__attribute__((naked))
__attribute__((aligned(4)))
void kernel_entry(void) {
    __asm__ __volatile__(
        // Retrieve the kernel stack of the running process from sscratch
        "csrrw sp, sscratch, sp\n"
        "addi sp, sp, -4 * 31\n"
        "sw ra,  4 * 0(sp)\n"
        "sw gp,  4 * 1(sp)\n"
        "sw tp,  4 * 2(sp)\n"
        "sw t0,  4 * 3(sp)\n"
        "sw t1,  4 * 4(sp)\n"
        "sw t2,  4 * 5(sp)\n"
        "sw t3,  4 * 6(sp)\n"
        "sw t4,  4 * 7(sp)\n"
        "sw t5,  4 * 8(sp)\n"
        "sw t6,  4 * 9(sp)\n"
        "sw a0,  4 * 10(sp)\n"
        "sw a1,  4 * 11(sp)\n"
        "sw a2,  4 * 12(sp)\n"
        "sw a3,  4 * 13(sp)\n"
        "sw a4,  4 * 14(sp)\n"
        "sw a5,  4 * 15(sp)\n"
        "sw a6,  4 * 16(sp)\n"
        "sw a7,  4 * 17(sp)\n"
        "sw s0,  4 * 18(sp)\n"
        "sw s1,  4 * 19(sp)\n"
        "sw s2,  4 * 20(sp)\n"
        "sw s3,  4 * 21(sp)\n"
        "sw s4,  4 * 22(sp)\n"
        "sw s5,  4 * 23(sp)\n"
        "sw s6,  4 * 24(sp)\n"
        "sw s7,  4 * 25(sp)\n"
        "sw s8,  4 * 26(sp)\n"
        "sw s9,  4 * 27(sp)\n"
        "sw s10, 4 * 28(sp)\n"
        "sw s11, 4 * 29(sp)\n"

        // Retrieve and save the sp at the time of exception
        "csrr a0, sscratch\n"
        "sw a0, 4 * 30(sp)\n"

        // Reset the kernel stack
        "addi a0, sp, 4 * 31\n"
        "csrw sscratch, a0\n"

        "mv a0, sp\n"
        "call handle_trap\n"

        "lw ra,  4 * 0(sp)\n"
        "lw gp,  4 * 1(sp)\n"
        "lw tp,  4 * 2(sp)\n"
        "lw t0,  4 * 3(sp)\n"
        "lw t1,  4 * 4(sp)\n"
        "lw t2,  4 * 5(sp)\n"
        "lw t3,  4 * 6(sp)\n"
        "lw t4,  4 * 7(sp)\n"
        "lw t5,  4 * 8(sp)\n"
        "lw t6,  4 * 9(sp)\n"
        "lw a0,  4 * 10(sp)\n"
        "lw a1,  4 * 11(sp)\n"
        "lw a2,  4 * 12(sp)\n"
        "lw a3,  4 * 13(sp)\n"
        "lw a4,  4 * 14(sp)\n"
        "lw a5,  4 * 15(sp)\n"
        "lw a6,  4 * 16(sp)\n"
        "lw a7,  4 * 17(sp)\n"
        "lw s0,  4 * 18(sp)\n"
        "lw s1,  4 * 19(sp)\n"
        "lw s2,  4 * 20(sp)\n"
        "lw s3,  4 * 21(sp)\n"
        "lw s4,  4 * 22(sp)\n"
        "lw s5,  4 * 23(sp)\n"
        "lw s6,  4 * 24(sp)\n"
        "lw s7,  4 * 25(sp)\n"
        "lw s8,  4 * 26(sp)\n"
        "lw s9,  4 * 27(sp)\n"
        "lw s10, 4 * 28(sp)\n"
        "lw s11, 4 * 29(sp)\n"
        "lw sp,  4 * 30(sp)\n"
        "sret\n"
    );
}

// Notifies the device that there is a new request. `desc_index` is the index
// of the head descriptor of the new request.
void virtq_kick(struct virtio_virtq *vq, int desc_index) {
    vq->avail.ring[vq->avail.index % VIRTQ_ENTRY_NUM] = desc_index;
    vq->avail.index++;
    __sync_synchronize();
    virtio_reg_write32(VIRTIO_REG_QUEUE_NOTIFY, vq->queue_index);
    vq->last_used_index++;
}

// Returns whether there are requests being processed by the device.
bool virtq_is_busy(struct virtio_virtq *vq) {
    return vq->last_used_index != *vq->used_index;
}

// Reads/writes from/to virtio-blk device.
void read_write_disk(void *buf, unsigned sector, int is_write) {
    if (sector >= blk_capacity / SECTOR_SIZE) {
        kprintf("virtio: tried to read/write sector=%d, but capacity is %d\n",
              sector, blk_capacity / SECTOR_SIZE);
        return;
    }

    // Construct the request according to the virtio-blk specification.
    blk_req->sector = sector;
    blk_req->type = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    if (is_write)
        memcpy(blk_req->data, buf, SECTOR_SIZE);

    // Construct the virtqueue descriptors (using 3 descriptors).
    struct virtio_virtq *vq = blk_request_vq;
    vq->descs[0].addr = blk_req_paddr;
    vq->descs[0].len = sizeof(uint32_t) * 2 + sizeof(uint64_t);
    vq->descs[0].flags = VIRTQ_DESC_F_NEXT;
    vq->descs[0].next = 1;

    vq->descs[1].addr = blk_req_paddr + offsetof(struct virtio_blk_req, data);
    vq->descs[1].len = SECTOR_SIZE;
    vq->descs[1].flags = VIRTQ_DESC_F_NEXT | (is_write ? 0 : VIRTQ_DESC_F_WRITE);
    vq->descs[1].next = 2;

    vq->descs[2].addr = blk_req_paddr + offsetof(struct virtio_blk_req, status);
    vq->descs[2].len = sizeof(uint8_t);
    vq->descs[2].flags = VIRTQ_DESC_F_WRITE;

    // Notify the device that there is a new request.
    virtq_kick(vq, 0);

    // Wait until the device finishes processing.
    while (virtq_is_busy(vq))
        ;

    // virtio-blk: If a non-zero value is returned, it's an error.
    if (blk_req->status != 0) {
        kprintf("virtio: warn: failed to read/write sector=%d status=%d\n",
               sector, blk_req->status);
        return;
    }

    // For read operations, copy the data into the buffer.
    if (!is_write)
        memcpy(buf, blk_req->data, SECTOR_SIZE);
}

struct file files[FILES_MAX];
uint8_t disk[DISK_MAX_SIZE];

int oct2int(char *oct, int len) {
    int dec = 0;
    for (int i = 0; i < len; i++) {
        if (oct[i] < '0' || oct[i] > '7')
            break;

        dec = dec * 8 + (oct[i] - '0');
    }
    return dec;
}

void fs_init(void) {
    for (unsigned sector = 0; sector < sizeof(disk) / SECTOR_SIZE; sector++)
        read_write_disk(&disk[sector * SECTOR_SIZE], sector, false);

    unsigned off = 0;
    for (int i = 0; i < FILES_MAX; i++) {
        struct tar_header *header = (struct tar_header *) &disk[off];
        if (header->name[0] == '\0')
            break;

        if (strcmp(header->magic, "ustar") != 0)
            PANIC("invalid tar header: magic=\"%s\"", header->magic);

        int filesz = oct2int(header->size, sizeof(header->size));
        struct file *file = &files[i];
        file->in_use = true;
        strcpy(file->name, header->name);
        memcpy(file->data, header->data, filesz);
        file->size = filesz;
        kprintf("file: %s, size=%d\n", file->name, file->size);

        off += align_up(sizeof(struct tar_header) + filesz, SECTOR_SIZE);
    }
}

void fs_flush(void) {
    // Copy all file contents into `disk` buffer.
    memset(disk, 0, sizeof(disk));
    unsigned off = 0;
    for (int file_i = 0; file_i < FILES_MAX; file_i++) {
        struct file *file = &files[file_i];
        if (!file->in_use)
            continue;

        struct tar_header *header = (struct tar_header *) &disk[off];
        memset(header, 0, sizeof(*header));
        strcpy(header->name, file->name);
        strcpy(header->mode, "000644");
        strcpy(header->magic, "ustar");
        strcpy(header->version, "00");
        header->type = '0';

        // Turn the file size into an octal string.
        int filesz = file->size;
        for (int i = sizeof(header->size); i > 0; i--) {
            header->size[i - 1] = (filesz % 8) + '0';
            filesz /= 8;
        }

        // Calculate the checksum.
        int checksum = ' ' * sizeof(header->checksum);
        for (unsigned i = 0; i < sizeof(struct tar_header); i++)
            checksum += (unsigned char) disk[off + i];

        for (int i = 5; i >= 0; i--) {
            header->checksum[i] = (checksum % 8) + '0';
            checksum /= 8;
        }

        // Copy file data.
        memcpy(header->data, file->data, file->size);
        off += align_up(sizeof(struct tar_header) + file->size, SECTOR_SIZE);
    }

    // Write `disk` buffer into the virtio-blk.
    for (unsigned sector = 0; sector < sizeof(disk) / SECTOR_SIZE; sector++)
        read_write_disk(&disk[sector * SECTOR_SIZE], sector, true);

    kprintf("wrote %d bytes to disk\n", sizeof(disk));
}

void probe_sbi_extension(long eid, const char * name) {
    struct sbiret value = sbi_call(eid, 0, 0, 0, 0, 0, 3, 0x10);
    kprintf("probe_extension[0x%x]: value=%s0x%x %s\033[0m\terror=%d\t(%s Extension)\n", eid, value.value ? "\033[32m" : "\033[31m", value.value, value.value ? "(available)    " : "(not available)", value.error, name);
}

long hart_get_status(long hartid) {
    struct sbiret value = sbi_call(hartid, 0, 0, 0, 0, 0, 0x2, 0x48534d);
    if (value.error == -3)
        return -3;
    kprintf("hart_get_status[%d] (Hart State Management Extension): value=0x%x %s\terror=%d %s\n",
        hartid, 
        value.value,
        value.value == 0
            ? "(started)        "
            : value.value == 1 ? "(stopped)        "
            : value.value == 2 ? "(start pending)  "
            : value.value == 3 ? "(stop pending)   "
            : value.value == 4 ? "(suspended)      "
            : value.value == 5 ? "(suspend pending)"
            : value.value == 6 ? "(resume pending) "
            :                    "(unknown)        ",
        value.error,
        value.error ==  0 ? "(no error)" : "(unknown)");
    return value.value;
}

static long boot_hart = -1, num_harts = -1;

typedef struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
} fdt_header;

typedef struct fdt_reserve_entry {
    uint64_t address;
    uint64_t size;
} fdt_reserve_entry;

enum FDT_TOKEN { FDT_BEGIN_NODE = 1, FDT_END_NODE, FDT_PROP, FDT_NOP, FDT_END };

typedef struct fdt_prop {
    uint32_t len;
    uint32_t nameoff;
} fdt_prop;

void print_property_stringlist(const char* string_list, uint32_t len) {
    printf("<stringlist> [");
    size_t inc = 0;
    do {
        printf("`\033[32m%s\033[0m`", string_list + inc);
        inc += strlen(string_list + inc) + 1;
        if (inc >= len)
            break;
        printf(", ");
    } while (true);
    putchar(']');
}

enum FDT_TOKEN *print_node(enum FDT_TOKEN *token, const char* strings, size_t depth) {
    for (size_t i = 0; i < depth; i++)
        printf("│");
    printf("╭─");
    bool cont = true;

    const char* name = (char*)((uint32_t)token + 4);
    uint32_t len = strlen(name);
    printf(" 0x%p /%s\n", token, name);
    token += 1 + (len + sizeof(token)) / sizeof(token);

    do {
        switch (be_to_le(*token)) {
        case FDT_BEGIN_NODE:
            for (size_t i = 0; i < depth + 1; i++)
                printf("│");
            putchar('\n');
            token = print_node(token, strings, depth + 1);
            break;

        case FDT_END_NODE:
            for (size_t i = 0; i < depth; i++)
                printf("│");
            printf("╰─ /%s\n", name);
            return token + 1;

        case FDT_PROP: {
            const fdt_prop *prop = (fdt_prop*)((uint32_t)token + 4);
            const char* name = strings+be_to_le(prop->nameoff);
            for (size_t i = 0; i < depth; i++)
                putchar('|');
            printf("├─╴ {len=%d} %s: ", be_to_le(prop->len), name);
            if (be_to_le(prop->len) == 0) {
                // Do nothing
                printf("<empty>");
            } else if (strcmp(name, "reg") == 0) {
                printf("\033[90mI don't understand this one at the moment...\033[0m", 0);
            } else if (strcmp(name, "#address-cells") == 0 || strcmp(name, "#size-cells") == 0 || strcmp(name, "phandle") == 0 || strcmp(name, "virtual-reg") == 0 || strcmp(name, "timebase-frequency") == 0) {
                /* U32 */
                printf("<u32> \033[36m%d\033[0m (\033[36m0x%x\033[0m)", be_to_le(*(uint32_t*)(prop + 1)));
            } else if (strcmp(name, "compatible") == 0) {
                /* STRINGLIST */
                print_property_stringlist((char*)(prop + 1), be_to_le(prop->len));
            } else if (strcmp(name, "model") == 0 || strcmp(name, "bootargs") == 0 || strcmp(name, "stdout-path") == 0 || strcmp(name, "device_type") == 0 || strcmp(name, "status") == 0) {
                /* STRING */
                const char* string = (char*)(prop + 1);
                printf("<string> `\033[32m%s\033[0m`", string);
            } else if (strcmp(name, "bank-width") == 0) {
                printf("\033[33m<Unknown custom property type!>\033[0m");
            } else {
                PANIC("\033[33m<Unhandled property type!>\033[0m");
                cont = false;
            }
            putchar('\n');
            uint32_t next_addr = ((uint32_t)(prop + 1)) + be_to_le(prop->len);
            if (next_addr % 4)
                next_addr += 4 - (next_addr % 4);
            token = (enum FDT_TOKEN*)(next_addr);
        } break;

        case FDT_NOP:
            token++;
            break;

        case FDT_END:
            kprintf("Token at 0x%p is FDT_END\n", token);
            for (size_t i = 0; i < depth; i++)
                printf("│");
            printf("╰─ /%s\n", name);
            return token + 1;

        default:
            // kprintf();
            PANIC("Token at 0x%p is unknown (0x%x)!\n", token, be_to_le(*token));
            break;
        }
    } while (cont);

    for (size_t i = 0; i < depth; i++)
        printf("│");
    printf("╰─ /%s (\033[31mwith errors\033[0m)\n", name);
    return token;
}

void inspect_device_tree(const fdt_header *fdt) {
    printf("\n");
    kprintf("Found FDT magic at 0x%x (%x), version %d\n", fdt, be_to_le(fdt->magic), be_to_le(fdt->version));
    kprintf("Boot CPU is /cpus/cpu@%d in the device tree...\n", be_to_le(fdt->boot_cpuid_phys));
    kprintf("FDT total size is %d bytes\n", be_to_le(fdt->totalsize));
    kprintf("Strings size is %d bytes\n", be_to_le(fdt->size_dt_strings));
    kprintf("Devicetree size is %d bytes\n", be_to_le(fdt->size_dt_struct));

    const fdt_reserve_entry *entries = (void*)((uint32_t)fdt + be_to_le(fdt->off_mem_rsvmap));
    kprintf("Reserved entries table starts %d bytes after the FDT header (at 0x%p).\n", be_to_le(fdt->off_mem_rsvmap), entries);
    long i = 0;
    kprintf("Entry at %d: %x (%d)\n", i, entries[i].address, entries[i].size);
    while ((entries[i].address + entries[i].size) != 0) ++i;
    kprintf("There are %d reserved memory ranges in the device tree.\n", i);

    const char* strings = (char*)((uint32_t)fdt + be_to_le(fdt->off_dt_strings));
    kprintf("Strings block starts %d bytes after the FDT header (at 0x%p).\n", be_to_le(fdt->off_dt_strings), strings);

    enum FDT_TOKEN *token = (enum FDT_TOKEN*)((uint32_t)fdt + be_to_le(fdt->off_dt_struct));
    kprintf("Device tree starts %d bytes after the FDT header (at 0x%p).\n", be_to_le(fdt->off_dt_struct), token);

    bool cont = false;
    do {
        token = print_node(token, strings, 0);
        // switch (be_to_le(*token)) {
        //     case FDT_BEGIN_NODE: {
        //         const char* name = (char*)((uint32_t)token + 4);
        //         uint32_t len = strlen(name);
        //         kprintf("Token at 0x%p is FDT_BEGIN_NODE: \"%s\" (len: %d)\n", token, name, len);
        //         // kprintf("Incrementing token by %d\n", 1 + (len+sizeof(token)) / sizeof(token));
        //         token += 1 + (len + sizeof(token)) / sizeof(token);
        //     } break;
        //     case FDT_END_NODE:
        //         kprintf("Token at 0x%p is FDT_END_NODE\n", token);
        //         token += 1;
        //         break;
        //     case FDT_PROP: {
        //         const fdt_prop *prop = (fdt_prop*)((uint32_t)token + 4);
        //         const char* name = strings+be_to_le(prop->nameoff);
        //         kprintf("Token at 0x%p is FDT_PROP {len=%d, nameoff=%d, name=`%s`}\n", token, be_to_le(prop->len), be_to_le(prop->nameoff), name);
        //         if (be_to_le(prop->len) == 0) {
        //             // Do nothing
        //         } else if (strcmp(name, "reg") == 0) {
        //             kprintf("\tI don't understand this one at the moment...\n", 0);
        //         } else if (strcmp(name, "#address-cells") == 0 || strcmp(name, "#size-cells") == 0) {
        //             uint32_t address_cells = be_to_le(*(uint32_t*)(prop + 1));
        //             kprintf("\tValue: %d\n", address_cells);
        //         } else if (strcmp(name, "compatible") == 0) {
        //             const char* string_list = (char*)(prop + 1);
        //             size_t inc = 0;
        //             do {
        //                 kprintf("\tCompatible: %s\n", string_list + inc);
        //                 inc += strlen(string_list + inc) + 1;
        //             } while (inc < be_to_le(prop->len));
        //         } else if (strcmp(name, "model") == 0) {
        //             const char* string = (char*)(prop + 1);
        //             kprintf("\tModel: %s\n", string);
        //         } else {
        //             cont = false;
        //             break;
        //         }
        //         uint32_t next_addr = ((uint32_t)(prop + 1)) + be_to_le(prop->len);
        //         if (next_addr % 4)
        //             next_addr += 4 - (next_addr % 4);
        //         token = (enum FDT_TOKEN*)(next_addr);
        //     } break;
        //     case FDT_NOP:
        //         kprintf("Token at 0x%p is FDT_NOP\n", token);
        //         cont = false;
        //         break;

        //     case FDT_END:
        //         kprintf("Token at 0x%p is FDT_END\n", token);
        //         cont = false;
        //         break;
        //     default:
        //         kprintf("Token at 0x%p is unknown (0x%x)!\n", token, be_to_le(*token));
        //         cont = false;
        //         break;
        // }
    } while (cont);

    printf("\n");
}

void kernel_main(long arg0, long arg1) {
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;

    memset(__bss, 0, (size_t) __bss_end - (size_t)__bss);
    WRITE_CSR(stvec, (uint32_t) kernel_entry);

    printf("\n\n");
    printf("\033[1;93m ______     ______     __         ______     __   __     ______     __       \n");
    printf("/\\  ___\\   /\\  __ \\   /\\ \\       /\\  __ \\   /\\ \"-.\\ \\   /\\  ___\\   /\\ \\      \n");
    printf("\\ \\ \\____  \\ \\ \\/\\ \\  \\ \\ \\____  \\ \\ \\/\\ \\  \\ \\ \\-.  \\  \\ \\  __\\   \\ \\ \\____ \n");
    printf(" \\ \\_____\\  \\ \\_____\\  \\ \\_____\\  \\ \\_____\\  \\ \\_\\\\\"\\_\\  \\ \\_____\\  \\ \\_____\\\n");
    printf("  \\/_____/   \\/_____/   \\/_____/   \\/_____/   \\/_/ \\/_/   \\/_____/   \\/_____/\033[0m\n\n");

    uint32_t time = READ_CSR(time);
    kprintf("CPU uptime: %d ticks. (%d.%ds?)\n", time, time / 10000000, (time % 10000000) / 10000);

    struct sbiret value = sbi_call(0, 0, 0, 0, 0, 0, 0, 0x10);
    kprintf("get_spec_version: value=0x%x\terror=%d\n", value.value, value.error);

    probe_sbi_extension(0x10,       "Base");
    probe_sbi_extension(0x4442434e, "\"DBCN\" Debug Console");
    probe_sbi_extension(0x48534d,   "\"HSM\" Hart State Management");
    probe_sbi_extension(0x53525354, "\"SRST\" System Reset");
    probe_sbi_extension(0x54494d45, "\"TIME\" Timer");

    for (long hartid = 0; hartid < MAX_HARTS; hartid++) {
        long status = hart_get_status(hartid);
        if (status == -3) {
            num_harts = hartid;
            break;
        }
        if (status == 0)
            boot_hart = hartid;

        if (hartid + 1 == MAX_HARTS)
            kprintf("There may be more than %d harts...\n", MAX_HARTS);
    }
    kprintf("There are %d harts, and the kernel is booting on hart #%d.\n", num_harts, boot_hart);


    if (*(unsigned long *)arg1 != 0xedfe0dd0)
        PANIC("Could not find magic device tree value!");
    // kprintf("Device tree location: a1=%x magic=%x\n", arg1, *(unsigned long *)arg1);
    inspect_device_tree((const fdt_header *)arg1);

    sbi_call(0x0, 0x0, 0, 0, 0, 0, 0, 0x53525354);
    PANIC("TODO");

    virtio_blk_init();

    fs_init();

    idle_proc = create_process(NULL, 0);
    idle_proc->pid = -1; // idle
    current_proc = idle_proc;

    create_process(_binary_shell_bin_start, (size_t)_binary_shell_bin_size);

    kprintf("Starting process %d...\n\n", 0);
    uint32_t start_time = READ_CSR(time);
    yield();
    
    // Shutdown?
    printf("[Idle Process] Calling system shutdown.\n");
    uint32_t ctime = READ_CSR(time);
    kprintf("Current time: (0x%x %x) %d\n", READ_CSR(timeh), ctime, ctime);
    uint32_t diff = ctime - start_time;
    kprintf("Process 0 ran for (0x%x) %d ticks. (%d.%ds?)\n", diff, diff, diff / 10000000, (diff % 10000000) / 10000);
    value = sbi_call(0x0, 0x0, 0, 0, 0, 0, 0, 0x53525354);
    kprintf("system_reset:\n\tvalue=0x%x\n\terror=%d\n", value.value, value.error);
    PANIC("failed to call system shutdown...");
}

struct file *fs_lookup(const char *filename) {
    for (int i = 0; i < FILES_MAX; i++) {
        struct file *file = &files[i];
        if (!strcmp(file->name, filename))
            return file;
    }

    return NULL;
}


void handle_syscall(struct trap_frame *f) {
    switch (f->a3) {
        case SYS_PUTCHAR:
            putchar(f->a0);
            break;
        case SYS_GETCHAR:
            while (1) {
                long ch = getchar();
                if (ch >= 0) {
                    f->a0 = ch;
                    break;
                }

                yield();
            }
            break;
        case SYS_EXIT:
            kprintf("process %d exited\n", current_proc->pid);
            current_proc->state = PROC_EXITED;
            yield();
            PANIC("unreachable");
        case SYS_READFILE:
        case SYS_WRITEFILE: {
            const char *filename = (const char *) f->a0;
            char *buf = (char *) f->a1;
            int len = f->a2;
            struct file *file = fs_lookup(filename);
            if (!file) {
                kprintf("file not found: %s\n", filename);
                f->a0 = -1;
                break;
            }

            if (len > (int) sizeof(file->data))
                len = file->size;

            if (f->a3 == SYS_WRITEFILE) {
                memcpy(file->data, buf, len);
                file->size = len;
                fs_flush();
            } else {
                memcpy(buf, file->data, len);
            }

            f->a0 = len;
            break;
        }
        default:
            PANIC("unexpected syscall a3=%x\n", f->a3);
    }
}

void handle_trap(struct trap_frame *f) {
    uint32_t scause = READ_CSR(scause);
    uint32_t stval = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);
    if (scause == SCAUSE_ECALL) {
        handle_syscall(f);
        user_pc += 4;
    } else {
        PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
    }

    WRITE_CSR(sepc, user_pc);
}

__attribute__((section(".text.boot")))
__attribute__((naked))
void boot(long arg0, long arg1) {
    __asm__ __volatile__ (
        "mv sp, %[stack_top]\n"  // Set the stack pointer
        // "addi sp, sp, -4\n"      // Move stack pointer down
        // "sw a0, (sp)\n"          // Push a0 onto stack
        // "addi sp, sp, -4\n"      // Move stack pointer down
        // "sw a1, (sp)\n"          // Push a1 onto stack
        "call kernel_main" // Jump to kernel_main with restored a0
        :
        : [stack_top] "r" (__stack_top)
        : "sp"
    );
}



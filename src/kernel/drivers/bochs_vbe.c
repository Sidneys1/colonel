#include <drivers/bochs_vbe.h>
#include <kernel.h>

enum BOCHS_VBE_INDEXES : uint8_t {
    BOCHS_VBE_XRES = 1,
    BOCHS_VBE_YRES,
    BOCHS_VBE_BPP,
    BOCHS_VBE_ENABLE,
    BOCHS_VBE_BANK,
    BOCHS_VBE_VIRT_WIDTH,
    BOCHS_VBE_VIRT_HEIGHT,
    BOCHS_VBE_X_OFFSET,
    BOCHS_VBE_Y_OFFSET,
};

enum BOCHS_VBE_ENABLE_VALUES : uint8_t {
    BVE_DISABLED = 0,
    BVE_ENABLED,
    BVE_GETCAPS,

    BVE_LFB_ENABLED = 0x40,
};

static uint16_t read16(const volatile void *addr) { return *((volatile uint16_t *)(addr)); }

static void write16(volatile void *addr, uint16_t value) { *((volatile uint16_t *)(addr)) = value; }

static void write8(volatile void *addr, uint8_t value) { *((volatile uint8_t *)(addr)) = value; }

static inline void *res2mmio(unsigned long offset, unsigned long mask) {
    return (void *)(paddr_t)((0x40000000 + offset) & ~mask);
}

static int bochs_read(int index) { return read16(res2mmio(0x500 + index * 2, 0)); }

static void bochs_write(int index, int val) { write16(res2mmio(0x500 + index * 2, 0), val); }

static void bochs_vga_write(int index, uint8_t val) { write8(res2mmio(0x400 + index, 0), val); }

void init_bochs(struct pci_type0_header *header) {
    paddr_t VGA_BUFFER = 0x41000000; // TODO: find spare memory
    // printf("\n\nMapping VGA framebuffer to memory at 0x%p\n", VGA_BUFFER);
    header->bars[0].raw = VGA_BUFFER | 0x08;
    header->pci_header.command = header->pci_header.command | 0x02;

    paddr_t VGA_REGS = 0x40000000; // TODO: find spare memory
    // printf("\n\nMapping VGA registers to memory at 0x%p\n", VGA_REGS);
    header->bars[2].raw = VGA_REGS;

    int id = bochs_read(0);
    if ((id & 0xfff0) != 0xB0C0) {
        kprintf("QEMU VGA: bochs dispi: ID mismatch (expected 0xBOC_, got %#04X).\n", id);
        return;
    }
    int mem = bochs_read(0xa) * 64 * 1024;
    kprintf("\033[36mQEMU VGA: Bochs VBE found, %d MiB video memory mapped at %p.\n", mem / (1024 * 1024), VGA_BUFFER);

    // printf("QEMU VGA: framebuffer @ %p (pci bar %d)\n",
    //        VGA_BUFFER, bar);

    /* setup video mode */
    bochs_write(BOCHS_VBE_ENABLE, BVE_GETCAPS);
    int xres = bochs_read(BOCHS_VBE_XRES), yres = bochs_read(BOCHS_VBE_YRES), bpp = bochs_read(BOCHS_VBE_BPP);
    bochs_write(BOCHS_VBE_ENABLE, BVE_DISABLED);
    bochs_write(BOCHS_VBE_BANK, 0);
    bochs_write(BOCHS_VBE_BPP, 32);
    bochs_write(BOCHS_VBE_XRES, 800);
    bochs_write(BOCHS_VBE_YRES, 600);
    bochs_write(BOCHS_VBE_VIRT_WIDTH, 800);
    bochs_write(BOCHS_VBE_VIRT_HEIGHT, 600);
    bochs_write(BOCHS_VBE_X_OFFSET, 0);
    bochs_write(BOCHS_VBE_Y_OFFSET, 0);
    bochs_write(BOCHS_VBE_ENABLE, BVE_ENABLED | BVE_LFB_ENABLED);

    bochs_vga_write(0, 0x20); /* disable blanking */

    uint32_t *z = (uint32_t *)VGA_BUFFER;
    for (int y = 0; y < 600; y++)
        for (int x = 0; x < 800; x++)
            //     0x00rrggbb
            z[(y * 800) + x] = x == y ? 0x00ffffff : ((800 - x) == y ? 0x00ffffff : 0x00000055);
}
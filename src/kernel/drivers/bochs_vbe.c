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

#define FONT_WIDTH 5
#define FONT_HEIGHT 6
#define FONT_BIT(x, y) (0b100000000000000000000000000000 >> ((y * FONT_WIDTH) + x))
#define FONT_CHAR_BIT(c, x, y) (font[c] & FONT_BIT(x, y))
#define FONT_NDEF 0b111111101110101110111111110101

const uint32_t font[256] = {
    /***** Control block (x31) *****/
    ['\0']={},{},{},{},{},{},{},['\a']={},
    ['\b']={},['\t']={},['\n']={},['\v']={},['\f']={},['\r']={},{},{},
    {},{},{},{},{},{},{},{},{},
    {},{},['\e']={},{},{},{},{},
    /***** Symbols *****/
    [' ']=0,
    ['!']=0b001000010000100000000010000000,
    ['"']=0b010100101000000000000000000000,
    ['#']=0b010101111101010111110101000000,
    ['$']=0b011101010001110001011111000000,
    ['%']=0b110011001000100010011001100000,
    ['&']=0b010001010001001101100110000000,
    ['\'']=0b001000010000000000000000000000,
    ['(']=0b000010001000010000100000100000,
    [')']=0b100000100001000010001000000000,
    ['*']=0b010100010001010000000000000000,
    ['+']=0b000000010001110001000000000000,
    [',']=0b000000000000000000000100010000,
    ['-']=0b000000000001110000000000000000,
    ['.']=0b000000000000000000001000000000,
    ['/']=0b000010001000100010001000000000,
    ['0']=0b011101000110101100010111000000,
    ['1']=0b001000110000100001000111000000,
    ['2']=0b011101000100110010001111100000,
    ['3']=0b011101000100110100010111000000,
    ['4']=0b100101001011111000100001000000,
    ['5']=0b111101000011110000011111000000,
    ['6']=0b011101000011110100010111000000,
    ['7']=0b111111000100010001000010000000,
    ['8']=0b011101000101110100010111000000,
    ['9']=0b011101000101111000010111000000,
    [':']=0b000000000001000000000100000000,
    [';']=0b000000000001000000000100010000,
    ['<']=0b001000100010000010000010000000,
    ['=']=0b000000111000000011100000000000,
    ['>']=0b001000001000001000100010000000,
    ['?']=0b011100001000100000000010000000,
    ['@']=0b011101010110110100010111000000,
    /***** Uppercase *****/
    ['A']=0b011101000111111100011000100000,
    ['B']=0b111101000111110100011111100000,
    ['C']=0b011101000110000100010111000000,
    ['D']=0b111101000110001100011111000000,
    ['E']=0b111111000011100100001111100000,
    ['F']=0b111111000011100100001000000000,
    ['G']=0b011111000010111100010111000000,
    ['H']=0b100011000111111100011000100000,
    ['I']=0b011100010000100001000111000000,
    ['J']=0b001110000100001100010111000000,
    ['K']=0b100011001011100100101000100000,
    ['L']=0b100001000010000100001111100000,
    ['M']=0b100011101110101100011000100000,
    ['N']=0b100011100110101100111000100000,
    ['O']=0b011101000110001100010111000000,
    ['P']=0b111101000111110100001000000000,
    ['Q']=0b011101000110001100100110100000,
    ['R']=0b111101000111110100011000100000,
    ['S']=0b011101000001110000011111000000,
    ['T']=0b111110010000100001000010000000,
    ['U']=0b100011000110001100010111000000,
    ['V']=0b100011000101010010100010000000,
    ['W']=0b100011000110101110111000100000,
    ['X']=0b100010101000100010101000100000,
    ['Y']=0b100010101000100001000010000000,
    ['Z']=0b111110001000100010001111100000,
    /***** Symbols, cont. *****/
    ['[']=0b000110001000010000100001100000,
    ['\\']=0b100000100000100000100000100000,
    [']']=0b110000100001000010001100000000,
    ['^']=0b001000101000000000000000000000,
    ['_']=0b000000000000000000000000011111,
    ['`']=0b001000001000000000000000000000,
    /***** Lowercase *****/
    ['a']=0b000000111110001100010111100000,
    ['b']=0b100001011011001100011111000000,
    ['c']=0b000000111010000100000111000000,
    ['d']=0b000010110110011100010111100000,
    ['e']=0b000000110011110100000111000000,
    ['f']=0b000110010001110001000010000000,
    ['g']=0b000000111110001011110000101110,
    ['h']=0b100001011011001100011000100000,
    ['i']=0b001000000000100001000010000000,
    ['j']=0b000100000000010000100101000100,
    ['k']=0b100001001010100111001001000000,
    ['l']=0b001000010000100001000001000000,
    ['m']=0b000001101010101101011010100000,
    ['n']=0b000000110001010010100101000000,
    ['o']=0b000000010001010010100010000000,
    ['p']=0b000001011011001100011111010000,
    ['q']=0b000000110110011100010111100001,
    ['r']=0b000000110001010010000100000000,
    ['s']=0b000000011001100000100110000000,
    ['t']=0b001000111000100001000001000000,
    ['u']=0b000000101001010010100011000000,
    ['v']=0b000000101001010001000010000000,
    ['w']=0b000001010110101101010101100000,
    ['x']=0b000000101001010001000101000000,
    ['y']=0b000000101001010001100001001100,
    ['z']=0b000001111000100010001111000000,
    /***** Symbols, pt 3! *****/
    ['{']=0b000110001000100000100001100000,
    ['|']=0b001000010000100001000010000000,
    ['}']=0b110000100000100010001100000000,
    ['~']=0b000000110110010000000000000000,
    0b111111101110101110111111110101,
};

const bool a[256][20] = {
    /***** Control block (x31) *****/
    {},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},
    /***** Symbols *****/
    // 0x20 - Space
    {},
    // 0x21 - ! through 0x2F - /
    {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    // 0x30 - 0 through 0x3f - ?
    {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    // 0x40 - @
    {},
    // 0x41 - A
    {0, 1, 1, 0,
     1, 0, 0, 1,
     1, 1, 1, 1,
     1, 0, 0, 1},
    // 0x42 - B
    {0, 1, 1, 0,
     1, 0, 1, 0,
     1, 0, 0, 1,
     1, 1, 1, 0},
    // 0x43 - C
    {0, 1, 1, 1,
     1, 0, 0, 0,
     1, 0, 0, 0,
     0, 1, 1, 1},
    // 0x44 - D
    {1, 1, 1, 0,
     1, 0, 0, 1,
     1, 0, 0, 1,
     1, 1, 1, 0},
    // 0x45 - E
    {1, 1, 1, 1,
     1, 0, 0, 0,
     1, 1, 1, 0,
     1, 1, 1, 1},
    // 0x46 - F
    {1, 1, 1, 1,
     1, 0, 0, 0,
     1, 1, 1, 0,
     1, 0, 0, 0},
    // 0x47 - G
    {0, 1, 1, 1,
     1, 0, 0, 0,
     1, 0, 0, 1,
     0, 1, 1, 1},
    // 0x48 - H
    {1, 0, 0, 1,
     1, 0, 0, 1,
     1, 1, 1, 1,
     1, 0, 0, 1},
    // 0x49 - I
    {0, 0, 1, 0,
     0, 0, 1, 0,
     0, 0, 1, 0,
     0, 0, 1, 0},
    // 0x4A - J
    {0, 1, 1, 0,
     0, 0, 1, 0,
     0, 0, 1, 0,
     0, 1, 0, 0},
    // 0x4B - K
    {1, 0, 0, 1,
     1, 0, 0, 1,
     1, 1, 1, 0,
     1, 0, 0, 1},
    // 0x4C - L
    {1, 0, 0, 0,
     1, 0, 0, 0,
     1, 0, 0, 0,
     1, 1, 1, 1},
    // 0x4D - M
    {1, 1, 0, 1,
     1, 0, 1, 1,
     1, 0, 0, 1,
     1, 0, 0, 1},
    // 0x4E - N
    {1, 0, 0, 1,
     1, 1, 0, 1,
     1, 0, 1, 1,
     1, 0, 0, 1},
    // 0x4F - O
    {0, 1, 1, 0,
     1, 0, 0, 1,
     1, 0, 0, 1,
     0, 1, 1, 0},
    // 0x50 - P
    {1, 1, 1, 0,
     1, 0, 0, 1,
     1, 1, 1, 0,
     1, 0, 0, 0},
    // 0x51 - Q
    {0, 1, 1, 0,
     1, 0, 0, 1,
     1, 0, 0, 1,
     0, 1, 1, 1,
     0, 0, 0, 1},
    // 0x52 - R
    {1, 1, 1, 0,
     1, 0, 0, 1,
     1, 1, 1, 0,
     1, 0, 0, 1},
    // 0x53 - S
    {0, 1, 1, 1,
     1, 1, 1, 0,
     0, 0, 0, 1,
     1, 1, 1, 0},
    // 0x54 - T
    {0, 1, 1, 1,
     0, 0, 1, 0,
     0, 0, 1, 0,
     0, 0, 1, 0},
    // 0x55 - U
    {1, 0, 0, 1,
     1, 0, 0, 1,
     1, 0, 0, 1,
     0, 1, 1, 0},
    // 0x56 - V
    {1, 0, 1, 0,
     1, 0, 1, 0,
     1, 1, 1, 0,
     0, 1, 0, 0},
    // 0x57 - W
    {1, 0, 0, 1,
     1, 0, 0, 1,
     0, 1, 1, 0,
     0, 1, 1, 0},
    // 0x58 - X
    {1, 0, 0, 1,
     0, 1, 1, 0,
     1, 0, 0, 1,
     1, 0, 0, 1},
    // 0x59 - Y
    {0, 1, 0, 1,
     0, 1, 0, 1,
     0, 0, 1, 0,
     0, 0, 1, 0},
    // 0x5A - Z
    {1, 1, 1, 1,
     0, 0, 1, 0,
     0, 1, 0, 0,
     1, 1, 1, 1},

};
const size_t foo = sizeof(a);

void write_char_at(unsigned char c, int x, int y) {
    const paddr_t VGA_BUFFER = 0x41000000;
    // const bool (*bitmap)[20] = &a[c - 'A'];
    uint32_t *z = (uint32_t *)VGA_BUFFER;
    for (int _y = 0; _y < FONT_HEIGHT; _y++)
    for (int _x = 0; _x < FONT_WIDTH; _x++)
        z[((y + _y) * 800) + x + _x] = FONT_CHAR_BIT(c, _x, _y) ? 0x00ffffff : (0x00000000);
}

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
    // int xres = bochs_read(BOCHS_VBE_XRES), yres = bochs_read(BOCHS_VBE_YRES), bpp = bochs_read(BOCHS_VBE_BPP);
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


    // for (int y = 0; y < 600; y++)
    //     for (int x = 0; x < 800; x++)
    //         //     0x00rrggbb
    //         z[(y * 800) + x] = x == y ? 0x00ffffff : ((800 - x) == y ? 0x00ffffff : 0x00000055);

    const char* message = "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG";
    for (const char *c = message; *c != '\0'; c++)
        write_char_at(*c, 100 + ((FONT_WIDTH + 1) * (c - message)), 100);
    message = "the quick brown fox jumps over the lazy dog";
    for (const char *c = message; *c != '\0'; c++)
        write_char_at(*c, 100 + ((FONT_WIDTH + 1) * (c - message)), 100 + FONT_HEIGHT + 1);

    message = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz";
    for (const char *c = message; *c != '\0'; c++)
        write_char_at(*c, 100 + ((FONT_WIDTH + 1) * (c - message)), 100 + ((FONT_HEIGHT + 1) * 2));

    message = "!\"#$%&'()*+,-./";
    for (const char *c = message; *c != '\0'; c++)
        write_char_at(*c, 100 + ((FONT_WIDTH + 1) * (c - message)), 100 + ((FONT_HEIGHT + 1) * 3));
    message = "0123456789";
    for (const char *c = message; *c != '\0'; c++)
        write_char_at(*c, 100 + ((FONT_WIDTH + 1) * (c - message)), 100 + ((FONT_HEIGHT + 1) * 4));
    message = ":;<=>?@[\\]^_`{|}~";
    for (const char *c = message; *c != '\0'; c++)
        write_char_at(*c, 100 + ((FONT_WIDTH + 1) * (c - message)), 100 + ((FONT_HEIGHT + 1) * 5));

    // char buffer[100] = {};

    // uint32_t time = 0;
    // for (int i = 0; i < 100000000; i++) {
    //     uint32_t start = READ_CSR(time);
    //     snprintf(buffer, 100, "Looping: %d; Last frame took %d.%03d ms to draw. ", i, time / 10000, (time % 10000) / 10);
    //     for (const char *c = buffer; *c != '\0'; c++)
    //         write_char_at(*c, 20 + ((FONT_WIDTH + 1) * (c - buffer)), 100 + ((FONT_HEIGHT + 1) * 6));
    //     time = READ_CSR(time) - start;

    //     WRITE_CSR(stimecmp, start + (CLOCK_FREQ / 1000));
    //     WAIT_FOR_INTERRUPT();

    //     // WRITE_CSR(sie, READ_CSR(sie) & ~0x200);
    //     // printf("Re-enabling interrupts...\n");
    //     // WRITE_CSR(sie, READ_CSR(sie) | 0x200);
    // }
// #define BALL_SIZE 30
//     uint32_t *z = (uint32_t *)VGA_BUFFER;
//     struct float2d {float x, y;} velocity = {4.0, 0}, position = {(BALL_SIZE + 1) / 2.0, (BALL_SIZE + 1) / 2.0};
//     for (int i = 0; i < 100000000; i++) {
//         // Erase
//         for (int y = -(BALL_SIZE / 2); y < (BALL_SIZE / 2); y++)
//         for (int x = -(BALL_SIZE / 2); x < (BALL_SIZE / 2); x++)
//             z[(int)(y + position.y) * 800 + (int)(x + position.x)] = 0;

//         // Move
//         position.x += velocity.x;
//         position.y += velocity.y;

//         // Gravity
//         if (velocity.y < 10)
//             velocity.y += 0.1;

//         if (position.x >= (800 - (BALL_SIZE + 4) / 2)) {
//             velocity.x *= -1.0;
//         }
//         if (position.y >= (600 - (BALL_SIZE + 2) / 2)) {
//             velocity.y *= -1.0;
//         }

//         if (position.x <= ((BALL_SIZE + 1) / 2)) {
//             velocity.x *= -1.0;
//         }
//         if (position.y <= ((BALL_SIZE + 1) / 2)) {
//             velocity.y *= -1.0;
//         }

//         // Draw
//         for (int y = -(BALL_SIZE / 2); y < (BALL_SIZE / 2); y++)
//         for (int x = -(BALL_SIZE / 2); x < (BALL_SIZE / 2); x++)
//             z[(int)(y + position.y) * 800 + (int)(x + position.x)] = 0x00ffffff;

//         WRITE_CSR(stimecmp, READ_CSR(time) + (CLOCK_FREQ / 60));
//         WAIT_FOR_INTERRUPT();
    // }


}

#include <common.h>
#include <console.h>
#include <devices/device_tree.h>
#include <devices/plic.h>
#include <devices/uart.h>
#include <harts.h>
#include <kernel.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <color.h>

#define MAX_NODE_NAME_LENGTH 1000

typedef struct fdt_reserve_entry {
    uint64_t address;
    uint64_t size;
} fdt_reserve_entry;

enum FDT_TOKEN {
    FDT_BEGIN_NODE = 1,
    FDT_END_NODE,
    FDT_PROP,
    FDT_NOP,
    FDT_END
};

typedef struct fdt_prop {
    uint32_t len;
    uint32_t nameoff;
} fdt_prop;

struct fdt_node {
    const char *name;
    struct fdt_node *next;
};

static void print_property_stringlist(const char *string_list, uint32_t len, bool color) {
    putchar('[');
    // printf("<stringlist> [");
    size_t inc = 0;
    do {
        printf(color ? "`" ANSI_GREEN "%S" ANSI_RESET "`" : "`%S`", string_list + inc);
        inc += strnlen_s(string_list + inc, MAX_NODE_NAME_LENGTH) + 1;
        if (inc >= len)
            break;
        printf(", ");
    } while (true);
    putchar(']');
}

enum FDT_TOKEN *print_node(struct fdt_node *root, enum FDT_TOKEN *token, const char *strings, size_t depth,
                           uint32_t address_cells, uint32_t size_cells) {
    for (size_t i = 0; i < depth; i++)
        printf("│  ");
    printf("├─");
    bool cont = true;

    const char *name = (char *)((uint32_t)token + 4);
    struct fdt_node self = {name, NULL};
    if (root == NULL)
        root = &self;

    uint32_t len = strnlen_s(name, MAX_NODE_NAME_LENGTH);
    printf(" 0x%p " ANSI_GREEN, token);
    struct fdt_node *ptr = root;
    while (ptr != &self) {
        printf("%S/", ptr->name);
        if (ptr->next == NULL)
            break;
        ptr = ptr->next;
    }
    printf("%S"ANSI_RESET"\n", root == &self ? "/" : name);

    if (ptr != &self)
        ptr->next = &self;

    token += 1 + (len + sizeof(token)) / sizeof(token);
    bool had_children = false;
    do {
        switch (be_to_le(*token)) {
        case FDT_BEGIN_NODE:
            had_children = true;
            token = print_node(root, token, strings, depth + 1, address_cells, size_cells);
            break;

        case FDT_END_NODE:
            if (had_children) {
                for (size_t i = 0; i < depth + 1; i++)
                    printf("│  ");
                printf("\n");
            }

            return token + 1;

        case FDT_PROP: {
            const fdt_prop *prop = (fdt_prop *)((uint32_t)token + 4);
            const char *name = strings + be_to_le(prop->nameoff);
            for (size_t i = 0; i < depth + 1; i++)
                printf("│  ");
            printf("├ prop {len=%d} %S: ", be_to_le(prop->len), name);
#define IS(x) strncmp(name, x, sizeof x) == 0
            if (be_to_le(prop->len) == 0) {
                // Do nothing
                printf("<empty>");
            } else if (IS("reg")) {
                printf("<cells> address="ANSI_CYAN"0x");
                uint32_t ai = 0;
                for (; ai < address_cells; ai++) {
                    uint32_t value = be_to_le(*(uint32_t *)(prop + 1 + ai));
                    printf("%x", value);
                }
                printf(""ANSI_RESET"");
                if (size_cells) {
                    printf(", size="ANSI_CYAN"0x");
                    for (uint32_t si = 0; si < size_cells; si++) {
                        uint32_t value = be_to_le(*(uint32_t *)(prop + 1 + ai + si));
                        printf("%x", value);
                    }
                }
                printf(""ANSI_RESET"");
                // printf(ANSI_RESET" - "ANSI_GREY"I don't understand this one at the moment..."ANSI_RESET"", 0);
            } else if (IS("#address-cells")) {
                uint32_t value = be_to_le(*(uint32_t *)(prop + 1));
                address_cells = value;
                printf("<u32> "ANSI_CYAN"%d"ANSI_RESET" ("ANSI_CYAN"0x%x"ANSI_RESET")", value, value);
            } else if (IS("#size-cells")) {
                uint32_t value = be_to_le(*(uint32_t *)(prop + 1));
                size_cells = value;
                printf("<u32> "ANSI_CYAN"%d"ANSI_RESET" ("ANSI_CYAN"0x%x"ANSI_RESET")", value, value);
            } else if (IS("virtual-reg") || IS("timebase-frequency") || IS("#interrupt-cells") ||
                       IS("clock-frequency") || IS("value") || IS("offset") || IS("riscv,ndev") || IS("regmap") ||
                       IS("linux,pci-domain") || IS("bank-width")) {
                /* U32 */
                printf("<u32> "ANSI_CYAN"%d"ANSI_RESET" ("ANSI_CYAN"0x%x"ANSI_RESET")", be_to_le(*(uint32_t *)(prop + 1)),
                       be_to_le(*(uint32_t *)(prop + 1)));
            } else if (IS("phandle") || IS("interrupt-parent") || IS("cpu")) {
                /* <phandle> */
                printf("<phandle> "ANSI_MAGENTA"0x%x"ANSI_RESET"", be_to_le(*(uint32_t *)(prop + 1)));
            } else if (IS("compatible")) {
                /* STRINGLIST */
                printf("<stringlist> ");
                print_property_stringlist((char *)(prop + 1), be_to_le(prop->len), true);
            } else if (IS("model") || IS("bootargs") || IS("stdout-path") || IS("device_type") || IS("status") ||
                       IS("mmu-type") || IS("riscv,isa")) {
                /* STRING */
                const char *string = (char *)(prop + 1);
                printf("<string> `"ANSI_GREEN"%S"ANSI_RESET"`", string);
            } else if (IS("interrupts") || IS("interrupt-map-mask") || IS("interrupt-map") || IS("ranges") ||
                       IS("bus-range") || IS("interrupts-extended")) {
                printf(ANSI_MAGENTA"<Unknown custom property type!>"ANSI_RESET"");
            } else {
                PANIC(ANSI_MAGENTA"<Unhandled property type!>"ANSI_RESET"");
                cont = false;
            }
#undef IS
            printf("\n");
            uint32_t next_addr = ((uint32_t)(prop + 1)) + be_to_le(prop->len);
            if (next_addr % 4)
                next_addr += 4 - (next_addr % 4);
            token = (enum FDT_TOKEN *)(next_addr);
        } break;

        case FDT_NOP:
            token++;
            break;

        case FDT_END:
            kprintf("Token at 0x%p is FDT_END\n", token);
            return token + 1;

        default:
            // kprintf();
            PANIC("Token at 0x%p is unknown (0x%x)!\n", token, be_to_le(*token));
            break;
        }
    } while (cont);

    for (size_t i = 0; i < depth; i++)
        printf("│");
    printf("╰─ /%S ("ANSI_RED"with errors"ANSI_RESET")\n", name);
    return token;
}

extern bool kernel_verbose;

static bool check_stringlist_contains(const char *string_list, const char *restrict cmp, uint32_t len) {
    size_t inc = 0;
    do {
        if (strncmp(string_list + inc, cmp, MAX_NODE_NAME_LENGTH) == 0)
            return true;
        inc += strnlen_s(string_list + inc, MAX_NODE_NAME_LENGTH) + 1;
        if (inc >= len)
            return false;
    } while (true);
}

#define IS(x) strncmp(name, x, sizeof x) == 0
enum FDT_TOKEN *traverse_node(struct fdt_node *root, enum FDT_TOKEN *token, const char *strings, uint32_t address_cells,
                              uint32_t size_cells) {
    bool cont = true;

    const char *node_name = (char *)((uint32_t)token + 4);
    struct fdt_node self = {node_name, NULL};
    if (root == NULL)
        root = &self;

    uint32_t len = strnlen_s(node_name, MAX_NODE_NAME_LENGTH);
    struct fdt_node *ptr = root;
    while (ptr != &self) {
        if (ptr->next == NULL)
            break;
        ptr = ptr->next;
    }
    if (ptr != &self)
        ptr->next = &self;

    token += 1 + (len + sizeof(token)) / sizeof(token);

    bool could_be_uart = node_name[0] == 'u' && node_name[1] == 'a' && node_name[2] == 'r' && node_name[3] == 't' && node_name[4] == '@';
    bool could_be_plic = node_name[0] == 'p' && node_name[1] == 'l' && node_name[2] == 'i' && node_name[3] == 'c' && node_name[4] == '@';

    do {
        switch (be_to_le(*token)) {
        case FDT_BEGIN_NODE:
            token = traverse_node(root, token, strings, address_cells, size_cells);
            break;

        case FDT_END_NODE:
            ptr->next = NULL;
            token++;
            cont = false;
            break;

        case FDT_PROP: {
            const fdt_prop *prop = (fdt_prop *)((uint32_t)token + 4);
            const char *name = strings + be_to_le(prop->nameoff);

            if (IS("bootargs") && strncmp(node_name, "chosen", 7) == 0) {
                if (strnlen_s((const char*)(prop + 1), 1))
                    kprintf("Found boot args: `%S`\n", (char *)(prop + 1));

                if (strncmp((char *)(prop + 1), "verbose", 8) == 0) {
                    kernel_verbose = true;
                    kprintf("Kernel is now in VERBOSE mode.\n");
                }
            } else if (IS("#address-cells")) {
                uint32_t value = be_to_le(*(uint32_t *)(prop + 1));
                address_cells = value;
            } else if (IS("#size-cells")) {
                uint32_t value = be_to_le(*(uint32_t *)(prop + 1));
                size_cells = value;
            } else if (IS("compatible")) {
                if (could_be_uart) {
                    could_be_uart = check_stringlist_contains((const char*)(prop + 1), "ns16550a", be_to_le(prop->len));
                    if (!could_be_uart) {
                        kprintf(ANSI_RED "Found incompatible UART: ");
                        printf(ANSI_RED);
                        print_property_stringlist((const char*)(prop + 1), be_to_le(prop->len), false);
                        printf(ANSI_RESET "\n");
                    }
                } else if (could_be_plic) {
                    could_be_plic = check_stringlist_contains((const char*)(prop + 1), "riscv,plic0", be_to_le(prop->len));
                    if (!could_be_plic) {
                        kprintf(ANSI_RED "Found incompatible PLIC: ");
                        printf(ANSI_RED);
                        print_property_stringlist((const char*)(prop + 1), be_to_le(prop->len), false);
                        printf(ANSI_RESET "\n");
                    }
                }
            }

            // printf("\n");
            uint32_t next_addr = ((uint32_t)(prop + 1)) + be_to_le(prop->len);
            if (next_addr % 4)
                next_addr += 4 - (next_addr % 4);
            token = (enum FDT_TOKEN *)(next_addr);
        } break;

        case FDT_NOP:
            token++;
            break;

        case FDT_END:
            // kprintf("Token at 0x%p is FDT_END\n", token);
            ptr->next = NULL;
            token++;
            cont = false;
            break;

        default:
            // kprintf();
            PANIC("Token at 0x%p is unknown (0x%x)!\n", token, be_to_le(*token));
            break;
        }
    } while (cont);

    if (could_be_uart) {
        const_string n = {.head = node_name + 5, .tail = node_name + len};
        if (uart_base != 0) {
            kprintf("Found additional UART device at 0x%s.\n", n);
        } else {
            uart_base = strtoul(n, 16);
        }
    }

    if (could_be_plic) {
        const_string n = {.head = node_name + 5, .tail = node_name + len};
        if (plic_base != 0) {
            kprintf("Found additional PLIC device at 0x%s.\n", n);
        } else {
            plic_base = strtoul(n, 16);
        }
    }

    // for (size_t i = 0; i < depth; i++)
    //     printf("│");
    // printf("╰─ /%S ("ANSI_RED"with errors"ANSI_RESET")\n", name);
    return token;
}
#undef IS

void device_tree_init(const fdt_header *fdt) {
    if (fdt->magic != 0xedfe0dd0)
        PANIC("Could not find FDT magic at 0x%p!\n", fdt);const char *strings = (char *)((uint32_t)fdt + be_to_le(fdt->off_dt_strings));
    enum FDT_TOKEN *token = (enum FDT_TOKEN *)((uint32_t)fdt + be_to_le(fdt->off_dt_struct));

    traverse_node(NULL, token, strings, 2, 1);
}

void inspect_device_tree(const fdt_header *fdt) {
    if (fdt->magic != 0xedfe0dd0)
        PANIC("Could not find FDT magic at 0x%p!\n", fdt);

    const char *strings = (char *)((uint32_t)fdt + be_to_le(fdt->off_dt_strings));
    enum FDT_TOKEN *token = (enum FDT_TOKEN *)((uint32_t)fdt + be_to_le(fdt->off_dt_struct));

    struct hart_local *hart = get_hart_local();
    hart->stdout->auto_flush = false;
    kprintf("Found FDT magic at 0x%x (%x), version %d\n", fdt, be_to_le(fdt->magic), be_to_le(fdt->version));
    kprintf("Boot CPU is /cpus/cpu@%ld in the device tree...\n", be_to_le(fdt->boot_cpuid_phys));
    kprintf("FDT total size is %d bytes\n", be_to_le(fdt->totalsize));
    kprintf("Strings size is %d bytes\n", be_to_le(fdt->size_dt_strings));
    kprintf("Devicetree size is %d bytes\n", be_to_le(fdt->size_dt_struct));
    const fdt_reserve_entry *entries = (void *)((uint32_t)fdt + be_to_le(fdt->off_mem_rsvmap));
    kprintf("Reserved entries table starts %d bytes after the FDT header (at 0x%p).\n",
            be_to_le(fdt->off_mem_rsvmap), entries);
    long i = 0;
    while ((entries[i].address + entries[i].size) != 0) {
        kprintf("Entry at %d: %x (%d)\n", i, entries[i].address, entries[i].size);
        ++i;
    }
    kprintf("There are %d reserved memory ranges in the device tree.\n", i);
    kprintf("Strings block starts %d bytes after the FDT header (at 0x%p).\n", be_to_le(fdt->off_dt_strings),
            strings);
    kprintf("Device tree starts %d bytes after the FDT header (at 0x%p).\n", be_to_le(fdt->off_dt_struct), token);

    print_node(NULL, token, strings, 0, 2, 1);
    hart->stdout->auto_flush = true;
    putchar('\n');
}

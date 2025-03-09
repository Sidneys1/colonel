#include <common.h>
#include <console.h>
#include <stddef.h>
#include <devices/device_tree.h>
#include <devices/plic.h>
#include <devices/uart.h>
#include <devices/virtio.h>
#include <devices/pci.h>
#include <harts.h>
#include <kernel.h>
#include <memory/slab_allocator.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <color.h>

#ifdef DEVICE_TREE_DEBUG
#define DT_DBG(...) KDBG(ANSI_MAGENTA "[DEVICE-TREE] " __VA_ARGS__)
#else
#define DT_DBG(...)
#endif

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
    uint32_t address_cells, size_cells;
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

enum FDT_TOKEN *print_node(struct fdt_node *root, struct fdt_node *parent, enum FDT_TOKEN *token, const char *strings, size_t depth) {
    for (size_t i = 0; i < depth; i++)
        printf("│  ");
    printf("├─");
    bool cont = true;

    const char *name = (char *)((uint32_t)token + 4);
    struct fdt_node self = {name, NULL, parent ? parent->address_cells : 2, parent ? parent->size_cells : 1};
    if (parent != NULL)
        parent->next = &self;
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
            token = print_node(root, &self, token, strings, depth + 1);
            self.next = NULL;
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
                printf("<cells[%lux%lu]> address="ANSI_CYAN"0x", self.address_cells, self.size_cells);
                uint32_t ai = 0;
                bool nz = false;
                for (; ai < self.address_cells; ai++) {
                    uint32_t value = be_to_le(*((uint32_t *)(prop + 1) + ai));
                    printf(nz ? "%08x" : "%x", value);
                    nz |= value;
                }
                printf(""ANSI_RESET"");
                if (self.size_cells) {
                    nz = false;
                    printf(", size="ANSI_CYAN"0x");
                    for (uint32_t si = 0; si < self.size_cells; si++) {
                        uint32_t value = be_to_le(*((uint32_t *)(prop + 1) + ai + si));
                        printf(nz ? "%08x" : "%x", value);
                        nz |= value;
                    }
                }
                printf(""ANSI_RESET"");
                // printf(ANSI_RESET" - "ANSI_GREY"I don't understand this one at the moment..."ANSI_RESET"", 0);
            } else if (IS("#address-cells")) {
                uint32_t value = be_to_le(*(uint32_t *)(prop + 1));
                self.address_cells = value;
                printf("<u32> "ANSI_CYAN"%d"ANSI_RESET" ("ANSI_CYAN"0x%x"ANSI_RESET")", value, value);
            } else if (IS("#size-cells")) {
                uint32_t value = be_to_le(*(uint32_t *)(prop + 1));
                self.size_cells = value;
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
            } else if (IS("compatible") || IS("riscv,isa-extensions")) {
                /* STRINGLIST */
                printf("<stringlist> ");
                print_property_stringlist((char *)(prop + 1), be_to_le(prop->len), true);
            } else if (IS("model") || IS("bootargs") || IS("stdout-path") || IS("device_type") || IS("status") ||
                       IS("mmu-type") || IS("riscv,isa") || IS("riscv,isa-base")) {
                /* STRING */
                const char *string = (char *)(prop + 1);
                printf("<string> `"ANSI_GREEN"%S"ANSI_RESET"`", string);
            } else if (IS("interrupts") || IS("interrupt-map-mask") || IS("interrupt-map") || IS("ranges") ||
                       IS("bus-range") || IS("interrupts-extended")) {
                printf(ANSI_MAGENTA"<Unknown custom property type!>"ANSI_RESET"");
            } else {
                printf(ANSI_MAGENTA"<Unhandled property type: `%S`!>"ANSI_RESET"", name);
                // cont = false;
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

static const char * check_stringlist_contains(const char *string_list, const char *restrict cmp, uint32_t len) {
    size_t inc = 0;
    do {
        if (strncmp(string_list + inc, cmp, MAX_NODE_NAME_LENGTH) == 0)
            return string_list + inc;
        inc += strnlen_s(string_list + inc, MAX_NODE_NAME_LENGTH) + 1;
        if (inc >= len)
            return NULL;
    } while (true);
}

struct compatible_device {
    const char *const compatible;
    void (*const initializer)(paddr_t);
    const uint8_t priority;
} const compatible_devices[] = {
    {.compatible="riscv,plic0", .initializer=plic_init, .priority=2},
    {.compatible="ns16550a", .initializer=uart_init, .priority=1},
    {.compatible="virtio,mmio", .initializer=probe_virtio_device, .priority=1},
    {.compatible="pci-host-ecam-generic", .initializer=probe_pci, .priority=1},
};

#define NUM_COMPAT_DEVICES (sizeof(compatible_devices) / sizeof(struct compatible_device))

struct device_node {
    struct device_node *next;
    const struct compatible_device *compatible;
    paddr_t address;
};

void check_compat(const const_string node_name, const fdt_prop *prop, struct device_node **head) {
    const struct compatible_device *compatible = NULL;
    for (size_t i = 0; i < NUM_COMPAT_DEVICES; i++) {
        if (check_stringlist_contains((const char*)(prop + 1), compatible_devices[i].compatible, be_to_le(prop->len)) != NULL) {
            compatible = &compatible_devices[i];
            break;
        }
    }

    if (compatible != NULL) {
        const char * c = node_name.head;
        while (*++c != '@' && *c != '\0' && c != node_name.tail);
        if (*c != '\0' && c != node_name.tail) {
            struct device_node *node = slab_malloc(struct device_node);
            node->compatible = compatible;
            node->address = strtoul((const_string){.head = c + 1, .tail = node_name.tail}, 16);
            DT_DBG("Adding device %s (%S), priority %hhu, to chain...\n", node_name, compatible->compatible, compatible->priority);
            if (*head == NULL) {
                DT_DBG("\tNo head, setting as head.\n");
                *head = node;
                node->next = NULL;
            } else if ((*head)->compatible->priority <= compatible->priority) {
                DT_DBG("\tHead priority was %hhu, but we're %hhu, so we're now the head.\n", (*head)->compatible->priority, compatible->priority);
                node->next = *head;
                *head = node;
            } else {
                DT_DBG("\tWe're a lower priority than head (%hhu), so we're searching for an entry point...\n", (*head)->compatible->priority);
                struct device_node *c = *head;
                size_t no = 1;
                while (c->next != NULL && c->next->compatible->priority > compatible->priority) {
                    DT_DBG("\t\tEntry #%zu priority was %hhu (> ours, %hhu), moving along...\n", ++no, c->next->compatible->priority, compatible->priority);
                    c = c->next;
                }
                DT_DBG("\t\tEntry #%zu priority is %hhu (<= than ours), so we're inserting before it.\n", no + 1, c->next->compatible->priority);
                node->next = c->next;
                c->next = node;
            }
        }
    }
}

#define IS(x) strncmp(name, x, sizeof x) == 0
enum FDT_TOKEN *traverse_node(struct fdt_node *parent, enum FDT_TOKEN *token, const char *strings, struct device_node **head) {
    bool cont = true;

    const char *node_name = (char *)((uint32_t)token + 4);
    struct fdt_node self = {node_name, NULL, parent ? parent->address_cells : 2, parent ? parent->size_cells : 1};
    if (parent != NULL)
        parent->next = &self;

    uint32_t len = strnlen_s(node_name, MAX_NODE_NAME_LENGTH);

    token += 1 + (len + sizeof(token)) / sizeof(token);
    do {
        switch (be_to_le(*token)) {
        case FDT_BEGIN_NODE:
            token = traverse_node(&self, token, strings, head);
            self.next = NULL;
            break;

        case FDT_END_NODE:
            token++;
            cont = false;
            break;

        case FDT_PROP: {
            const fdt_prop *prop = (fdt_prop *)((uint32_t)token + 4);
            const char *name = strings + be_to_le(prop->nameoff);

            if (IS("bootargs") && strncmp(node_name, "chosen", 7) == 0) {
                if (strnlen_s((const char*)(prop + 1), 1)) {
                    kprintf("Found boot args: `%S`\n", (char *)(prop + 1));
                    bootargs = (const_string){.head=(char*)(prop+1),.tail=(char*)(prop+1) + be_to_le(prop->len)};
                }

                if (strncmp((char *)(prop + 1), "verbose", 8) == 0) {
                    kernel_verbose = true;
                    kprintf("Kernel is now in VERBOSE mode.\n");
                }
            } else if (IS("#address-cells")) {
                uint32_t value = be_to_le(*(uint32_t *)(prop + 1));
                self.address_cells = value;
            } else if (IS("#size-cells")) {
                uint32_t value = be_to_le(*(uint32_t *)(prop + 1));
                self.size_cells = value;
            } else if (IS("compatible")) {
                check_compat((const const_string){.head=node_name, .tail=node_name + len}, prop, head);
            }

            uint32_t next_addr = ((uint32_t)(prop + 1)) + be_to_le(prop->len);
            if (next_addr % 4)
                next_addr += 4 - (next_addr % 4);
            token = (enum FDT_TOKEN *)(next_addr);
        } break;

        case FDT_NOP:
            token++;
            break;

        case FDT_END:
            token++;
            cont = false;
            break;

        default:
            PANIC("Token at 0x%p is unknown (0x%x)!\n", token, be_to_le(*token));
            break;
        }
    } while (cont);
    return token;
}
#undef IS

void device_tree_init(const fdt_header *fdt) {
    if (fdt->magic != 0xedfe0dd0)
        PANIC("Could not find FDT magic at 0x%p!\n", fdt);const char *strings = (char *)((uint32_t)fdt + be_to_le(fdt->off_dt_strings));
    enum FDT_TOKEN *token = (enum FDT_TOKEN *)((uint32_t)fdt + be_to_le(fdt->off_dt_struct));

    struct device_node *head = NULL;

    traverse_node(NULL, token, strings, &head);

    if (head != NULL) {
        struct device_node *c = head;
        do {
            DT_DBG("Found compatible device `%S` at %p\n", c->compatible->compatible, c->address);
            c->compatible->initializer(c->address);
            struct device_node *next = c->next;
            slab_free(&root_slab16, c);
            c = next;
        } while (c != NULL);
    }
}

void inspect_device_tree(const fdt_header *fdt) {
    if (fdt->magic != 0xedfe0dd0)
        PANIC("Could not find FDT magic at 0x%p!\n", fdt);
    const char *strings = (char *)((uint32_t)fdt + be_to_le(fdt->off_dt_strings));
    enum FDT_TOKEN *token = (enum FDT_TOKEN *)((uint32_t)fdt + be_to_le(fdt->off_dt_struct));

    // printf("idt\n");
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

    print_node(NULL, NULL, token, strings, 0);
    hart->stdout->auto_flush = true;
    putchar('\n');
    // flush();
}

#undef DT_DBG

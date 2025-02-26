#include <kernel.h>
#include <common.h>
#include <devices/device_tree.h>
#include <stdio.h>

typedef struct fdt_reserve_entry {
    uint64_t address;
    uint64_t size;
} fdt_reserve_entry;

enum FDT_TOKEN { FDT_BEGIN_NODE = 1, FDT_END_NODE, FDT_PROP, FDT_NOP, FDT_END };

typedef struct fdt_prop {
    uint32_t len;
    uint32_t nameoff;
} fdt_prop;

struct fdt_node {
    const char* name;
    struct fdt_node *next;
};

static void print_property_stringlist(const char* string_list, uint32_t len) {
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

enum FDT_TOKEN *print_node(struct fdt_node *root, enum FDT_TOKEN *token, const char* strings, size_t depth, uint32_t address_cells, uint32_t size_cells) {
    for (size_t i = 0; i < depth; i++)
        printf("│  ");
    printf("├─");
    bool cont = true;

    const char* name = (char*)((uint32_t)token + 4);
    struct fdt_node self = {name, NULL};
    if (root == NULL) root = &self;

    uint32_t len = strlen(name);
    printf(" 0x%p \033[32m", token);
    struct fdt_node *ptr = root;
    while (ptr != &self) {
        printf("%s/", ptr->name);
        if (ptr->next == NULL) break;
        ptr = ptr->next;
    }
    printf("%s\033[0m\n", root == &self ? "/" : name);

    if (ptr != &self) ptr->next = &self;

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
            const fdt_prop *prop = (fdt_prop*)((uint32_t)token + 4);
            const char* name = strings+be_to_le(prop->nameoff);
            for (size_t i = 0; i < depth + 1; i++)
                printf("│  ");
            printf("├ prop {len=%d} %s: ", be_to_le(prop->len), name);
#define IS(x) strcmp(name, x) == 0
            if (be_to_le(prop->len) == 0) {
                // Do nothing
                printf("<empty>");
            } else if (IS("reg")) {
                printf("<cells> address=\033[36m0x");
                uint32_t ai = 0;
                for (; ai < address_cells; ai++) {
                    uint32_t value = be_to_le(*((uint32_t*)(prop + 1) + ai));
                    if (ai == 0 && value == 0) continue;
                    printf("%x", value);
                }
                printf("\033[0m");
                if (size_cells) {
                    printf(", size=\033[36m0x");
                    for (uint32_t si = 0; si < size_cells; si++) {
                        uint32_t value = be_to_le(*((uint32_t*)(prop + 1) + ai + si));
                        if (si == 0 && value == 0) continue;
                        printf("%x", value);
                    }
                }
                printf("\033[0m");
                // printf("\033[0m - \033[90mI don't understand this one at the moment...\033[0m", 0);
            } else if (IS("#address-cells")) {
                uint32_t value = be_to_le(*(uint32_t*)(prop + 1));
                address_cells = value;
                printf("<u32> \033[36m%d\033[0m (\033[36m0x%x\033[0m)", value, value);
            } else if(IS("#size-cells")) {
                uint32_t value = be_to_le(*(uint32_t*)(prop + 1));
                size_cells = value;
                printf("<u32> \033[36m%d\033[0m (\033[36m0x%x\033[0m)", value, value);
            } else if (IS("ranges")) {
                uint32_t off = sizeof(fdt_prop);
                printf("<ranges> [");
                uint32_t reps = (be_to_le(prop->len) / (address_cells + address_cells + size_cells)) - 1;
                for (uint32_t i = 0; i <= reps; i++) {
                    printf("child-bus-address=\033[36m0x");
                    uint32_t ai = 0;
                    for (; ai < address_cells; ai++) {
                        uint32_t value = be_to_le(*((uint32_t*)prop + off + ai));
                        if (ai == 0 && value == 0) continue;
                        printf("%x", value);
                    }
                    printf("\033[0m");
                    printf(",parent-bus-address=\033[36m0x");
                    uint32_t ai2 = 0;
                    for (; ai2 < address_cells; ai2++) {
                        uint32_t value = be_to_le(*((uint32_t*)prop + off + ai + ai2));
                        if (ai2 == 0 && value == 0) continue;
                        printf("%x", value);
                    }
                    printf("\033[0m");
                    uint32_t si = 0;
                    if (size_cells) {
                        printf(",length=\033[36m0x");
                        for (; si < size_cells; si++) {
                            uint32_t value = be_to_le(*((uint32_t*)prop + off + ai + ai2 + si));
                            if (si == 0 && value == 0) continue;
                            printf("%x", value);
                        }
                    }
                    printf("\033[0m");
                    off += ai + ai2 + si;
                    if (i != reps)
                        printf(", ");
                }
                putchar(']');
            } else if (IS("virtual-reg") || IS("timebase-frequency") || IS("#interrupt-cells") || IS("clock-frequency") || IS("value") || IS("offset") || IS("riscv,ndev") || IS("regmap") || IS("linux,pci-domain") || IS("bank-width")) {
                /* U32 */
                printf("<u32> \033[36m%d\033[0m (\033[36m0x%x\033[0m)", be_to_le(*(uint32_t*)(prop + 1)), be_to_le(*(uint32_t*)(prop + 1)));
            } else if (IS("phandle") || IS("interrupt-parent") || IS("cpu")) {
                /* <phandle> */
                printf("<phandle> \033[35m0x%x\033[0m", be_to_le(*(uint32_t*)(prop + 1)));
            } else if (IS("compatible")) {
                /* STRINGLIST */
                print_property_stringlist((char*)(prop + 1), be_to_le(prop->len));
            } else if (IS("model") || IS("bootargs") || IS("stdout-path") || IS("device_type") || IS("status") || IS("mmu-type") || IS("riscv,isa")) {
                /* STRING */
                const char* string = (char*)(prop + 1);
                printf("<string> `\033[32m%s\033[0m`", string);
            } else if (IS("interrupts")|| IS("interrupt-map-mask") || IS("interrupt-map") || IS("bus-range")|| IS("interrupts-extended")) {
                printf("\033[33m<Unknown custom property type!>\033[0m");
            } else {
                printf("\033[33m<Unhandled property type!>\033[0m");
                // cont = false;
            }
#undef IS
            printf("\n");
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

extern bool kernel_verbose;

#define IS(x) strcmp(name, x) == 0
enum FDT_TOKEN *traverse_node(struct fdt_node *root, enum FDT_TOKEN *token, const char* strings, uint32_t address_cells, uint32_t size_cells) {
    bool cont = true;

    const char* node_name = (char*)((uint32_t)token + 4);
    struct fdt_node self = {node_name, NULL};
    if (root == NULL) root = &self;

    uint32_t len = strlen(node_name);
    struct fdt_node *ptr = root;
    while (ptr != &self) {
        if (ptr->next == NULL) break;
        ptr = ptr->next;
    }
    if (ptr != &self) ptr->next = &self;

    token += 1 + (len + sizeof(token)) / sizeof(token);
    do {
        switch (be_to_le(*token)) {
        case FDT_BEGIN_NODE:
            token = traverse_node(root, token, strings, address_cells, size_cells);
            break;

        case FDT_END_NODE:
            ptr->next = NULL;
            return token + 1;

        case FDT_PROP: {
            const fdt_prop *prop = (fdt_prop*)((uint32_t)token + 4);
            const char* name = strings+be_to_le(prop->nameoff);

            if (IS("bootargs") && strcmp(node_name, "chosen") == 0) {
                kprintf("Found boot args! `%s`\n", (char*)(prop + 1));
                if (strcmp((char*)(prop + 1), "verbose") == 0) {
                    kernel_verbose = true;
                    kprintf("Kernel is now in VERBOSE mode.\n");
                }
            } else if (IS("#address-cells")) {
                uint32_t value = be_to_le(*(uint32_t*)(prop + 1));
                address_cells = value;
            } else if(IS("#size-cells")) {
                uint32_t value = be_to_le(*(uint32_t*)(prop + 1));
                size_cells = value;
            }

            // printf("\n");
            uint32_t next_addr = ((uint32_t)(prop + 1)) + be_to_le(prop->len);
            if (next_addr % 4)
                next_addr += 4 - (next_addr % 4);
            token = (enum FDT_TOKEN*)(next_addr);
        } break;

        case FDT_NOP:
            token++;
            break;

        case FDT_END:
            // kprintf("Token at 0x%p is FDT_END\n", token);
            ptr->next = NULL;
            return token + 1;

        default:
            // kprintf();
            PANIC("Token at 0x%p is unknown (0x%x)!\n", token, be_to_le(*token));
            break;
        }
    } while (cont);

    // for (size_t i = 0; i < depth; i++)
    //     printf("│");
    // printf("╰─ /%s (\033[31mwith errors\033[0m)\n", name);
    return token;
}
#undef IS

void inspect_device_tree(const fdt_header *fdt) {
    if (fdt->magic != 0xedfe0dd0)
        PANIC("Could not find FDT magic at 0x%p!\n", fdt);

    const char* strings = (char*)((uint32_t)fdt + be_to_le(fdt->off_dt_strings));
    enum FDT_TOKEN *token = (enum FDT_TOKEN*)((uint32_t)fdt + be_to_le(fdt->off_dt_struct));

    traverse_node(NULL, token, strings, 2, 1);
    if (kernel_verbose) {
        kprintf("Found FDT magic at 0x%x (%x), version %d\n", fdt, be_to_le(fdt->magic), be_to_le(fdt->version));
        kprintf("Boot CPU is /cpus/cpu@%d in the device tree...\n", be_to_le(fdt->boot_cpuid_phys));
        kprintf("FDT total size is %d bytes\n", be_to_le(fdt->totalsize));
        kprintf("Strings size is %d bytes\n", be_to_le(fdt->size_dt_strings));
        kprintf("Devicetree size is %d bytes\n", be_to_le(fdt->size_dt_struct));
        const fdt_reserve_entry *entries = (void*)((uint32_t)fdt + be_to_le(fdt->off_mem_rsvmap));
        kprintf("Reserved entries table starts %d bytes after the FDT header (at 0x%p).\n", be_to_le(fdt->off_mem_rsvmap), entries);
        long i = 0;
        while ((entries[i].address + entries[i].size) != 0) {
            kprintf("Entry at %d: %x (%d)\n", i, entries[i].address, entries[i].size);
            ++i;
        }
        kprintf("There are %d reserved memory ranges in the device tree.\n", i);
        kprintf("Strings block starts %d bytes after the FDT header (at 0x%p).\n", be_to_le(fdt->off_dt_strings), strings);
        kprintf("Device tree starts %d bytes after the FDT header (at 0x%p).\n", be_to_le(fdt->off_dt_struct), token);

        print_node(NULL, token, strings, 0, 2, 1);
    }

    printf("\n");
}
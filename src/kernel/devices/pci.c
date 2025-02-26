
#include <common.h>
#include <devices/pci.h>
#include <kernel.h>
#include <memory/slab_allocator.h>
#include <sbi/sbi.h>

#include <drivers/bochs_vbe.h>

struct pci_driver drivers[] = {
    {BOCHS_VENDOR_ID, BOCHS_DEVICE_ID, &init_bochs},
};

struct pci_ll *pci_ll_head = NULL, *pci_ll_tail = NULL, *bridges[256] = {};

void print_pci_bar(struct pci_type0_header *type0, char num) {
    struct bar *bar = &type0->bars[num];
    // TODO: if ((type0->pci_header.command & 0x02) == 0x02)
    //           PANIC("<gotta disable>\n");

    uint32_t orig = bar->raw;
    bar->raw = 0xFFFFFFFF;
    uint32_t width = ((~(bar->raw & 0xFFFFFFF0)) + 1);
    bar->raw = orig;

    if (width == 0)
        return;

    printf("\t\tBAR %d [0x%p] (Raw: 0x%x)\t", num, bar, bar->raw);

    uint32_t value = bar->raw;
    struct bar_packed *barp = (struct bar_packed *)&value;

    if (barp->is_io_space)
        printf("\tIO-space\t", &bar);
    else
        printf("\tMemory-space\t", &bar);

    printf(barp->prefetchable ? "Prefetchable\t" : "");

    switch (barp->type) {
    case 0x0:
        printf("32-bit\t");
        break;
    case 0x2:
        printf("64-bit\t");
        break;
    case 0x3:
        PANIC("RESERVED!");
        break;
    }

    printf("Base: [0x%x]\t(Raw: 0x%x)\n", bar->raw & 0xFFFFFFF0, value);

    // uint32_t save = bar.raw;
    if (width >= 1024 * 1024)
        printf("\t\t\tSize: %dMiB\n", width >> 20);
    else if (width >= 1024)
        printf("\t\t\tSize: %dKiB\n", width >> 10);
    else
        printf("\t\t\tSize: %dB\n", width);
    // printf("\n\n%x\n\n", width);

    // *(uint32_t *)0x10000000 = 0xa0a0a0a0;

    // type0->bar0.raw = (uint32_t)pages;
    // printf("\nASDF (0x%p): 0x%x\n", &type0->bar0.raw, value & 0xFFFFFFF0);
    // printf(
    //    "\t\t     BAR0: 0x%s\t ...\n"
    //    "\t\tSubsys ID: 0x%x\t Sub. Vend: 0x%x\n", type0->bar0.prefetchable ? "Prefetchable" : "",
    //    type0->subsystem_id, type0->subsystem_vendor_id);
}

bool probe_pci_device(paddr_t base, uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    paddr_t address = base | ((uint32_t)bus << 20) | ((uint32_t)slot << 15) | ((uint32_t)func << 12) | (offset & 0xfc);
    struct pci_header *device_header = (struct pci_header *)address;
    if (device_header->device_id == 0xffff)
        return false;
    // printf("Slot #%d@0x%p:"
    //             "\tDevice ID: 0x%x\tVendor ID: 0x%x\n"
    //         "\t\t   Status: %d  \t  Command: %d\n"
    //         "\t\t    Class: 0x%x\t Subclass: 0x%x\t  Prog IF: %d  \t    Revision ID: %d\n"
    //         "\t\t     BIST: %d  \t Header T: %d  \tLatency T: %d  \tCache Line Size: %d\n",
    //         slot, address,
    //         device_header->device_id, device_header->vendor_id, device_header->status, device_header->command,
    //         device_header->class_code, device_header->subclass, device_header->prog_if, device_header->revision_id,
    //         device_header->bist, device_header->header_type, device_header->latency_timer,
    //         device_header->cache_line_size);
    // if (device_header->header_type == 0x0) {
    // struct pci_type0_header *type0 = (struct pci_type0_header*)device_header;

    // for (char bar = 0; bar < 6; bar++)
    //     print_pci_bar(type0, bar);
    // }
    // putchar('\n');

    if (device_header->class_code == 0x06) {
        struct pci_ll *bridge = (struct pci_ll *)slab_alloc(&root_slab16);
        bridge->next = NULL;
        // bridge->prev = pci_ll_tail;
        pci_ll_tail = bridge;
        if (pci_ll_head == NULL)
            pci_ll_head = bridge;
        bridge->device.pci = device_header;

        if (slot == 0)
            bridges[bus] = bridge;
    } else {
        struct pci_ll *device = (struct pci_ll *)slab_alloc(&root_slab16);
        device->next = NULL;
        struct pci_ll *bridge = bridges[bus];
        struct pci_ll *tail = bridge->first_child;
        if (bridge->first_child == NULL)
            bridge->first_child = device;
        else {
            for (;; tail = tail->next)
                if (tail->next == NULL)
                    break;
            tail->next = device;
        }
        device->device.pci = device_header;
    }
    return true;
}

void probe_pci(paddr_t base) {
    if (sizeof(struct pci_ll) > 16)
        PANIC("PCI Linked List entry is too big for a slab16 (%d > 16)!\n", sizeof(struct pci_ll));
    kprintf("Beginning PCI enumeration...\n");
    for (uint16_t bus = 0; bus < 256; bus++)
        for (uint16_t slot = 0; slot < 256; slot++)
            for (uint16_t func = 0; func < 256; func++)
                if (!probe_pci_device(base, bus, slot, func, 0))
                    break;

    for (size_t i = 0; i < 256 && bridges[i] != NULL; i++) {
        char *bridge_type, *vendor_id, *device_id;
        switch (bridges[i]->device.pci->subclass) {
        case 0x00:
            bridge_type = "Host";
            break;
        default:
            bridge_type = "unkown-type";
            break;
        }
        switch (bridges[i]->device.pci->vendor_id) {
        case 0x1b36:
            vendor_id = "Red Hat, Inc.";
            break;
        default:
            vendor_id = "unkown-vendor";
            break;
        }
        switch (bridges[i]->device.pci->device_id) {
        case 0x0008:
            device_id = "QEMU PCIe Host bridge";
            break;
        default:
            device_id = "unkown-device";
            break;
        }
        kprintf("[Bus %d] [%x:%x] %s Bridge (%s %s).\n", i, bridges[i]->device.pci->vendor_id,
                bridges[i]->device.pci->device_id, bridge_type, vendor_id, device_id);
        bridges[i]->device.pci->command = bridges[i]->device.pci->command | 0x06;
        for (struct pci_ll *child = bridges[i]->first_child; child != NULL; child = child->next) {
            switch (child->device.pci->class_code) {
            case 0x0:
                bridge_type = child->device.pci->subclass ? "VGA-compatible unclassified device" : "Unclssified device";
                break;
            case 0x1: {
                switch (child->device.pci->subclass) {
                case 0x0:
                    bridge_type = "SCSI bus controller";
                    break;
                case 0x1: {
                    // TODO: switch on progIF
                    bridge_type = "IDE controller";
                } break;
                case 0x2:
                    bridge_type = "Floppy disk controller";
                    break;
                case 0x3:
                    bridge_type = "IPI bus controller";
                    break;
                case 0x4:
                    bridge_type = "RAID contoller";
                    break;
                case 0x5: {
                    // TODO: switch on progIF
                    bridge_type = "ATA controller";
                } break;
                case 0x6: {
                    // TODO: switch on progIF
                    bridge_type = "SATA controller";
                } break;
                case 0x7: {
                    // TODO: switch on progIF
                    bridge_type = "SAS controller";
                } break;
                case 0x8: {
                    // TODO: switch on progIF
                    bridge_type = "Non-volatile memory controller (NVMHCI/NVMe)";
                } break;
                default:
                    bridge_type = "Generic mass storage controller";
                    break;
                }
            };
            case 0x03: {
                switch (child->device.pci->subclass) {
                case 0x0:
                    bridge_type = "VGA display controller";
                    break;
                case 0x1:
                    bridge_type = "XGA display controller";
                    break;
                case 0x2:
                    bridge_type = "3D display controller (non-VGA)";
                    break;
                default:
                    bridge_type = "Generic display controller";
                    break;
                }
            } break;
            default:
                bridge_type = "Unknown device";
                break;
            }
            switch (child->device.pci->vendor_id) {
            case BOCHS_VENDOR_ID:
                vendor_id = "BOCHS";
                break;
            default:
                vendor_id = "unkown-vendor";
                break;
            }
            switch (child->device.pci->device_id) {
            case BOCHS_DEVICE_ID:
                device_id = "Graphics Adaptor";
                break;
            default:
                device_id = "unkown-device";
                break;
            }
            kprintf("   | [%04hx:%04hx] %s (%s %s)\n", child->device.pci->vendor_id, child->device.pci->device_id,
                    bridge_type, vendor_id, device_id);
        }
    }

    for (size_t i = 0; i < 256 && bridges[i] != NULL; i++) {
        for (struct pci_ll *child = bridges[i]->first_child; child != NULL; child = child->next) {
            for (unsigned int i = 0; i < (sizeof(drivers) / sizeof(struct pci_driver)); i++) {
                if (child->device.pci->vendor_id != drivers[i].vendor_id ||
                    child->device.pci->device_id != drivers[i].device_id)
                    continue;
                drivers[i].init((struct pci_type0_header *)child->device.pci);
                break;
            }
        }
    }
}
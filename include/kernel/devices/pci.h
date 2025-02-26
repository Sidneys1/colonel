#pragma once

#include <stddef.h>

struct pci_header {
    uint16_t vendor_id, device_id, command, status;
    uint8_t revision_id, prog_if, subclass, class_code, cache_line_size, latency_timer, header_type, bist;
} __attribute__((packed));

struct bar_packed {
    bool is_io_space : 1;
    uint8_t type : 2;
    bool prefetchable : 1;
    uint32_t base_address : 28;
};

struct bar {
    union {
        volatile uint32_t raw;
        volatile struct bar_packed packed;
    };
} __attribute__((packed));

struct pci_type0_header {
    struct pci_header pci_header;
    struct bar bars[6];
    uint32_t cardbus_cis_pointer;
    uint16_t subsystem_vendor_id, subsystem_id;
    uint32_t expansion_rom_base_address;
    uint8_t capabilities_pointer;
    uint16_t reserved;
    uint8_t reserved2;
    uint32_t reserved3;
    uint8_t interrupt_line, interrupt_pin, min_grant, max_latency;
} __attribute__((packed));

// Size: 4 (for now)
struct pci_device {
    struct pci_header *pci;
} __attribute__((packed));

// Size: 12 + sizeof(struct pci_device) (4, for now, so 16 total)
struct pci_ll {
    struct pci_ll *next /*, *prev*/;
    struct pci_ll *first_child;
    struct pci_device device;
} __attribute__((packed));

void probe_pci(paddr_t base);

struct pci_driver {
    uint16_t vendor_id, device_id;
    void (*init)(struct pci_type0_header *);
};

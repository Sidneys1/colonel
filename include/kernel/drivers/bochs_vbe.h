#pragma once

#include <devices/pci.h>

#define BOCHS_VENDOR_ID 0x1234
#define BOCHS_DEVICE_ID 0x1111

void init_bochs(struct pci_type0_header *header);
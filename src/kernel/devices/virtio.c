#include "stddef.h"
#include <kernel.h>
#include <devices/virtio.h>
#include <memory/page_allocator.h>
#include <stdio.h>
#include <string.h>

struct virtio_virtq *blk_request_vq;
struct virtio_blk_req *blk_req;
paddr_t blk_req_paddr;
unsigned blk_capacity;
struct file files[FILES_MAX];
uint8_t disk[DISK_MAX_SIZE];

uint32_t virtio_reg_read32(unsigned offset) { return *((volatile uint32_t *)(VIRTIO_BLK_PADDR + offset)); }

uint64_t virtio_reg_read64(unsigned offset) { return *((volatile uint64_t *)(VIRTIO_BLK_PADDR + offset)); }

void virtio_reg_write32(unsigned offset, uint32_t value) {
    *((volatile uint32_t *)(VIRTIO_BLK_PADDR + offset)) = value;
}

void virtio_reg_fetch_and_or32(unsigned offset, uint32_t value) {
    virtio_reg_write32(offset, virtio_reg_read32(offset) | value);
}

void probe_virtio_device(paddr_t location) {
    uint32_t magic = *((volatile uint32_t*)(location + VIRTIO_REG_MAGIC));
    kprintf("Probing virtio device at 0x%p: magic=0x%x (%s)\n", location, magic, magic == 0x74726976 ? "matches" : "does not match");
    uint32_t reg_version = *((volatile uint32_t*)(location + VIRTIO_REG_VERSION));
    kprintf("\tversion=0x%x (%s)\n", reg_version, reg_version == 0x1 ? "legacy" : "v2");
    uint32_t device_id = *((volatile uint32_t*)(location + VIRTIO_REG_DEVICE_ID));
    char *did_name = "reserved";
    switch (device_id) {
    case 0: break;
    case 1: did_name = "network card"; break;
    case 2: did_name = "block device"; break;
    case 3: did_name = "console"; break;
    case 4: did_name = "entropy source"; break;
    case 5: did_name = "memory ballooning (traditional)"; break;
    case 6: did_name = "ioMemory"; break;
    case 7: did_name = "rpmsg"; break;
    case 8: did_name = "SCSI host"; break;
    case 9: did_name = "9P transport"; break;
    case 10: did_name = "mac80211 wlan"; break;
    case 11: did_name = "rproc serial"; break;
    case 12: did_name = "virtio CAIF"; break;
    case 13: did_name = "memory balloon"; break;
    case 16: did_name = "GPU device"; break;
    case 17: did_name = "Timer/Clock device"; break;
    case 18: did_name = "Input device"; break;
    case 19: did_name = "Socket device"; break;
    case 20: did_name = "Crypto device"; break;
    case 21: did_name = "Signal Distribution Module"; break;
    case 22: did_name = "pstore device"; break;
    case 23: did_name = "IOMMU device"; break;
    case 24: did_name = "Memory device "; break;
    default: did_name = "invalid"; break;
    }
    kprintf("\tdevice_id=0x%x (%s)\n", device_id, did_name);
}

struct virtio_virtq *virtq_init(unsigned index) {
    // Allocate a region for the virtqueue.
    paddr_t virtq_paddr = alloc_pages(align_up(sizeof(struct virtio_virtq), PAGE_SIZE) / PAGE_SIZE);
    struct virtio_virtq *vq = (struct virtio_virtq *)virtq_paddr;
    vq->queue_index = index;
    vq->used_index = (volatile uint16_t *)&vq->used.index;
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
    blk_req = (struct virtio_blk_req *)blk_req_paddr;
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
bool virtq_is_busy(struct virtio_virtq *vq) { return vq->last_used_index != *vq->used_index; }

// Reads/writes from/to virtio-blk device.
void read_write_disk(void *buf, unsigned sector, int is_write) {
    if (sector >= blk_capacity / SECTOR_SIZE) {
        kprintf("virtio: tried to read/write sector=%d, but capacity is %d\n", sector, blk_capacity / SECTOR_SIZE);
        return;
    }

    // Construct the request according to the virtio-blk specification.
    blk_req->sector = sector;
    blk_req->type = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    if (is_write)
        memcpy_s(blk_req->data, sizeof blk_req->data, buf, SECTOR_SIZE);

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
        kprintf("virtio: warn: failed to read/write sector=%d status=%d\n", sector, blk_req->status);
        return;
    }

    // For read operations, copy the data into the buffer.
    if (!is_write)
        memcpy_s(buf, SECTOR_SIZE, blk_req->data, SECTOR_SIZE);
}

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
        struct tar_header *header = (struct tar_header *)&disk[off];
        if (header->name[0] == '\0')
            break;

        if (strncmp(header->magic, "ustar", 6) != 0)
            PANIC("invalid tar header: magic=\"%S\"", header->magic);

        int filesz = oct2int(header->size, sizeof(header->size));
        struct file *file = &files[i];
        file->in_use = true;
        strncpy_s(file->name, sizeof file->name, header->name, sizeof header->name);
        if ((unsigned int)filesz > sizeof file->data)
            PANIC("Cannot load file `%S`, because it is larger than the available buffer (%d vs %d)!\n", file->name,
                  filesz, sizeof file->data);
        memcpy_s(file->data, sizeof file->data, header->data, filesz);
        file->size = filesz;
        kprintf("file: %S, size=%d\n", file->name, file->size);

        off += align_up(sizeof(struct tar_header) + filesz, SECTOR_SIZE);
    }
}

void fs_flush(void) {
    // Copy all file contents into `disk` buffer.
    memset_s(disk, sizeof disk, 0, sizeof disk);
    unsigned off = 0;
    for (int file_i = 0; file_i < FILES_MAX; file_i++) {
        struct file *file = &files[file_i];
        if (!file->in_use)
            continue;

        struct tar_header *header = (struct tar_header *)&disk[off];
        memset_s(header, sizeof *header, 0, sizeof *header);
        strncpy_s(header->name, sizeof header->name, file->name, sizeof file->name);
        strncpy_s(header->mode, sizeof header->mode, "000644", sizeof "000644");
        strncpy_s(header->magic, sizeof header->magic, "ustar", sizeof "ustar");
        strncpy_s(header->version, sizeof header->version, "00", sizeof "00");
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
            checksum += (unsigned char)disk[off + i];

        for (int i = 5; i >= 0; i--) {
            header->checksum[i] = (checksum % 8) + '0';
            checksum /= 8;
        }

        // Copy file data.
        memcpy_s(header->data, (sizeof disk) - off, file->data, file->size);
        off += align_up(sizeof(struct tar_header) + file->size, SECTOR_SIZE);
    }

    // Write `disk` buffer into the virtio-blk.
    for (unsigned sector = 0; sector < sizeof(disk) / SECTOR_SIZE; sector++)
        read_write_disk(&disk[sector * SECTOR_SIZE], sector, true);

    kprintf("wrote %d bytes to disk\n", sizeof(disk));
}

struct file *fs_lookup(const char *filename) {
    for (int i = 0; i < FILES_MAX; i++) {
        struct file *file = &files[i];
        if (!strncmp(file->name, filename, sizeof file->name))
            return file;
    }

    return NULL;
}

#pragma once

#include <common.h>
#include <kernel.h>
#include <stddef.h>

#define SECTOR_SIZE                512
#define VIRTQ_ENTRY_NUM            16
#define VIRTIO_DEVICE_BLK          2
#define VIRTIO_BLK_PADDR           0x10001000
#define VIRTIO_REG_MAGIC           0x00
#define VIRTIO_REG_VERSION         0x04
#define VIRTIO_REG_DEVICE_ID       0x08
#define VIRTIO_REG_QUEUE_SEL       0x30
#define VIRTIO_REG_QUEUE_NUM_MAX   0x34
#define VIRTIO_REG_QUEUE_NUM       0x38
#define VIRTIO_REG_QUEUE_ALIGN     0x3c
#define VIRTIO_REG_QUEUE_PFN       0x40
#define VIRTIO_REG_QUEUE_READY     0x44
#define VIRTIO_REG_QUEUE_NOTIFY    0x50
#define VIRTIO_REG_DEVICE_STATUS   0x70
#define VIRTIO_REG_DEVICE_CONFIG   0x100
#define VIRTIO_STATUS_ACK          1
#define VIRTIO_STATUS_DRIVER       2
#define VIRTIO_STATUS_DRIVER_OK    4
#define VIRTIO_STATUS_FEAT_OK      8
#define VIRTQ_DESC_F_NEXT          1
#define VIRTQ_DESC_F_WRITE         2
#define VIRTQ_AVAIL_F_NO_INTERRUPT 1
#define VIRTIO_BLK_T_IN            0
#define VIRTIO_BLK_T_OUT           1

// Virtqueue Descriptor area entry.
struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

// Virtqueue Available Ring.
struct virtq_avail {
    uint16_t flags;
    uint16_t index;
    uint16_t ring[VIRTQ_ENTRY_NUM];
} __attribute__((packed));

// Virtqueue Used Ring entry.
struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

// Virtqueue Used Ring.
struct virtq_used {
    uint16_t flags;
    uint16_t index;
    struct virtq_used_elem ring[VIRTQ_ENTRY_NUM];
} __attribute__((packed));

// NOLINTBEGIN
// Virtqueue.
struct __attribute__((packed)) virtio_virtq {
    struct virtq_desc descs[VIRTQ_ENTRY_NUM];
    struct virtq_avail avail;
    struct virtq_used used __attribute__((aligned(PAGE_SIZE)));
    int queue_index;
    volatile uint16_t *used_index;
    uint16_t last_used_index;
};
// NOLINTEND

// Virtio-blk request.
struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
    uint8_t data[512];
    uint8_t status;
} __attribute__((packed));

#define FILES_MAX     1
// #define DISK_MAX_SIZE align_up(sizeof(struct file) * FILES_MAX, SECTOR_SIZE)
#define DISK_MAX_SIZE align_up(77280, SECTOR_SIZE)

struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char type;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
    char data[]; // Array pointing to the data area following the header
                 // (flexible array member)
} __attribute__((packed));

struct file {
    bool in_use;      // Indicates if this file entry is in use
    char name[100];   // File name
    char data[82720]; // File content
    size_t size;      // File size
};

void fs_init(void);
void fs_flush(void);
void virtio_blk_init(void);
void read_write_disk(void *buf, unsigned sector, int is_write);
struct file *fs_lookup(const char *);

void probe_virtio_device(paddr_t location);

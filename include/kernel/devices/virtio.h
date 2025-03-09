#pragma once

#include <common.h>
#include <kernel.h>
#include <stddef.h>

#include <io.h>

#define VIRTQ_ENTRY_NUM            16
#define VIRTIO_DEVICE_BLK          2
// #define VIRTIO_BLK_PADDR           0x10001000
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

enum VIRTIO_DEVICE_IDS {
    VIRTIO_DEVICE_NETWORK_CARD = 1,
    VIRTIO_DEVICE_BLOCK = 2,
    VIRTIO_DEVICE_CONSOLE = 3,
    VIRTIO_DEVICE_ENTROPY_SOURCE = 4,
    VIRTIO_DEVICE_MEMORY_BALLOONING_TRADITIONAL = 5,
    VIRTIO_DEVICE_IOMEMORY = 6,
    VIRTIO_DEVICE_RPMSG = 7,
    VIRTIO_DEVICE_SCSI_HOST = 8,
    VIRTIO_DEVICE_9P_TRANSPORT = 9,
    VIRTIO_DEVICE_MAC80211_WLAN = 10,
    VIRTIO_DEVICE_RPROC_SERIAL = 11,
    VIRTIO_DEVICE_VIRTIO_CAIF = 12,
    VIRTIO_DEVICE_MEMORY_BALLOON = 13,
    VIRTIO_DEVICE_GPU_DEVICE = 16,
    VIRTIO_DEVICE_TIMER_CLOCK = 17,
    VIRTIO_DEVICE_INPUT = 18,
    VIRTIO_DEVICE_SOCKET = 19,
    VIRTIO_DEVICE_CRYPTO = 20,
    VIRTIO_DEVICE_SIGNAL_DISTRIBUTION_MODULE = 21,
    VIRTIO_DEVICE_PSTORE = 22,
    VIRTIO_DEVICE_IOMMU = 23,
    VIRTIO_DEVICE_MEMORY = 24,
};

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


// #define DISK_MAX_SIZE align_up(sizeof(struct file) * FILES_MAX, SECTOR_SIZE)
// #define DISK_MAX_SIZE align_up(4194304, SECTOR_SIZE)

struct virtio_device {
    struct virtio_device *next;
    paddr_t base_addr;
    struct virtio_virtq *queue;
    enum VIRTIO_DEVICE_IDS device_type;
};

struct virtio_blk_device {
    INHERITS(struct block_device);
    // INHERITS(struct virtio_device);
    struct virtio_device virtio;
    // struct virtio_device virtio;
    struct virtio_blk_req *requests;
    uint32_t sector_count;
};

struct virtio_blk_device* virtio_blk_init(paddr_t);
void read_write_disk(struct virtio_blk_device *dev, void *buf, unsigned sector, int is_write);

void probe_virtio_device(paddr_t location);

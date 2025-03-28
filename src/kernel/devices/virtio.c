#include <color.h>
#include <devices/virtio.h>
#include <drivers/filesystems/fat.h>
#include <drivers/filesystems/ustar.h>
#include <io.h>
#include <kernel.h>
#include <memory/page_allocator.h>
#include <memory/slab_allocator.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline uint32_t virtio_reg_read32(paddr_t base, unsigned offset) {
    return *((volatile uint32_t *)(base + offset));
}

static inline uint64_t virtio_reg_read64(paddr_t base, unsigned offset) {
    return *((volatile uint64_t *)(base + offset));
}

static inline void virtio_reg_write32(paddr_t base, unsigned offset, uint32_t value) {
    *((volatile uint32_t *)(base + offset)) = value;
}

static inline void virtio_reg_fetch_and_or32(paddr_t base, unsigned offset, uint32_t value) {
    virtio_reg_write32(base, offset, virtio_reg_read32(base, offset) | value);
}

void probe_virtio_device(paddr_t base) {
    if (*((volatile uint32_t *)(base + VIRTIO_REG_MAGIC)) != 0x74726976) {
        kprintf(ANSI_RED "Given address (%p) was not a virtio device!\n", base);
        return;
    }

    uint32_t reg_version = *((volatile uint32_t *)(base + VIRTIO_REG_VERSION));
    if (reg_version != 0x1) {
        kprintf(ANSI_RED "Virtio device version (%#010x) unsupported.\n", reg_version);
        return;
    }

    uint32_t device_id = *((volatile uint32_t *)(base + VIRTIO_REG_DEVICE_ID));

    switch (device_id) {
    case 0:
        return;
    case VIRTIO_DEVICE_BLOCK:
        kprintf("Found virtio block device.\n");
        add_block_device(SUPER(*virtio_blk_init(base)));
        break;
    default: {
        const_string did_name;
        switch (device_id) {
        case VIRTIO_DEVICE_NETWORK_CARD:
            did_name = CSTR("network card");
            break;
        case VIRTIO_DEVICE_CONSOLE:
            did_name = CSTR("console");
            break;
        case VIRTIO_DEVICE_ENTROPY_SOURCE:
            did_name = CSTR("entropy source");
            break;
        case VIRTIO_DEVICE_MEMORY_BALLOONING_TRADITIONAL:
            did_name = CSTR("memory ballooning (traditional)");
            break;
        case VIRTIO_DEVICE_IOMEMORY:
            did_name = CSTR("ioMemory");
            break;
        case VIRTIO_DEVICE_RPMSG:
            did_name = CSTR("rpmsg");
            break;
        case VIRTIO_DEVICE_SCSI_HOST:
            did_name = CSTR("SCSI host");
            break;
        case VIRTIO_DEVICE_9P_TRANSPORT:
            did_name = CSTR("9P transport");
            break;
        case VIRTIO_DEVICE_MAC80211_WLAN:
            did_name = CSTR("mac80211 wlan");
            break;
        case VIRTIO_DEVICE_RPROC_SERIAL:
            did_name = CSTR("rproc serial");
            break;
        case VIRTIO_DEVICE_VIRTIO_CAIF:
            did_name = CSTR("virtio CAIF");
            break;
        case VIRTIO_DEVICE_MEMORY_BALLOON:
            did_name = CSTR("memory balloon");
            break;
        case VIRTIO_DEVICE_GPU_DEVICE:
            did_name = CSTR("GPU device");
            break;
        case VIRTIO_DEVICE_TIMER_CLOCK:
            did_name = CSTR("Timer/Clock device");
            break;
        case VIRTIO_DEVICE_INPUT:
            did_name = CSTR("Input device");
            break;
        case VIRTIO_DEVICE_SOCKET:
            did_name = CSTR("Socket device");
            break;
        case VIRTIO_DEVICE_CRYPTO:
            did_name = CSTR("Crypto device");
            break;
        case VIRTIO_DEVICE_SIGNAL_DISTRIBUTION_MODULE:
            did_name = CSTR("Signal Distribution Module");
            break;
        case VIRTIO_DEVICE_PSTORE:
            did_name = CSTR("pstore device");
            break;
        case VIRTIO_DEVICE_IOMMU:
            did_name = CSTR("IOMMU device");
            break;
        case VIRTIO_DEVICE_MEMORY:
            did_name = CSTR("Memory device ");
            break;
        default:
            did_name = CSTR("unknown");
            break;
        }
        kprintf("Virtio device-type %u (%s) not supported.\n", device_id, did_name);
    }
        return;
    }
}

struct virtio_virtq *virtq_init(paddr_t base, unsigned index) {
    // Allocate a region for the virtqueue.
    paddr_t virtq_paddr = alloc_pages(align_up(sizeof(struct virtio_virtq), PAGE_SIZE) / PAGE_SIZE);
    struct virtio_virtq *vq = (struct virtio_virtq *)virtq_paddr; // slab_malloc(struct virtio_virtq);
    vq->queue_index = index;
    vq->used_index = (volatile uint16_t *)&vq->used.index;

    // 1. Select the queue writing its index (first queue is 0) to QueueSel.
    virtio_reg_write32(base, VIRTIO_REG_QUEUE_SEL, index);

    // 5. Notify the device about the queue size by writing the size to QueueNum.
    virtio_reg_write32(base, VIRTIO_REG_QUEUE_NUM, VIRTQ_ENTRY_NUM);

    // 6. Notify the device about the used alignment by writing its value in bytes to QueueAlign.
    virtio_reg_write32(base, VIRTIO_REG_QUEUE_ALIGN, 0);

    // 7. Write the physical number of the first page of the queue to the QueuePFN register.
    virtio_reg_write32(base, VIRTIO_REG_QUEUE_PFN, virtq_paddr);

    return vq;
}

size_t virtio_read_block(const struct block_device *dev, void *restrict tgt, size_t sector, size_t count) {
    // IS_SUBCLASS(*dev, struct virtio_blk_device);
    for (size_t i = 0; i < count; i++)
        read_write_disk((struct virtio_blk_device *)dev, tgt + i * SECTOR_SIZE, sector + i, 0);
    return count;
}

struct virtio_blk_device *virtio_blk_init(paddr_t base) {
    struct virtio_blk_device *device = slab_malloc(struct virtio_blk_device);
    device->super.read_block = virtio_read_block;
    device->virtio.next = NULL;
    device->virtio.base_addr = base;
    device->virtio.device_type = VIRTIO_DEVICE_BLOCK;

    char (*buffer)[16] = (char (*)[16])slab_malloc(struct { char _[16]; });
    snprintf(*buffer, 16, "virtio@%08x", base);
    device->super.id = *buffer;

    // 1. Reset the device.
    virtio_reg_write32(base, VIRTIO_REG_DEVICE_STATUS, 0);
    // 2. Set the ACKNOWLEDGE status bit: the guest OS has noticed the device.
    virtio_reg_fetch_and_or32(base, VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_ACK);
    // 3. Set the DRIVER status bit.
    virtio_reg_fetch_and_or32(base, VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER);
    // 5. Set the FEATURES_OK status bit.
    virtio_reg_fetch_and_or32(base, VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_FEAT_OK);

    // 7. Perform device-specific setup, including discovery of virtqueues for the device
    device->virtio.queue = virtq_init(base, 0);

    // 8. Set the DRIVER_OK status bit.
    virtio_reg_write32(base, VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER_OK);

    // Get the disk capacity.
    device->sector_count = virtio_reg_read64(base, VIRTIO_REG_DEVICE_CONFIG + 0);
    kprintf("virtio-blk: capacity is %d bytes\n", device->sector_count * SECTOR_SIZE);

    // Allocate a region to store requests to the device.
    device->requests =
        (struct virtio_blk_req *)alloc_pages(align_up(sizeof(struct virtio_blk_req), PAGE_SIZE) / PAGE_SIZE);

    return device;
}

// Notifies the device that there is a new request. `desc_index` is the index
// of the head descriptor of the new request.
void virtq_kick(paddr_t base, struct virtio_virtq *vq, int desc_index) {
    vq->avail.ring[vq->avail.index % VIRTQ_ENTRY_NUM] = desc_index;
    vq->avail.index++;
    __sync_synchronize();
    virtio_reg_write32(base, VIRTIO_REG_QUEUE_NOTIFY, vq->queue_index);
    vq->last_used_index++;
}

// Returns whether there are requests being processed by the device.
static inline bool virtq_is_busy(struct virtio_virtq *vq) { return vq->last_used_index != *vq->used_index; }

// Reads/writes from/to virtio-blk device.
void read_write_disk(struct virtio_blk_device *dev, void *buf, unsigned sector, int is_write) {
    if (sector >= dev->sector_count) {
        kprintf("virtio: tried to read/write sector=%d, but capacity is %d\n", sector, dev->sector_count);
        return;
    }

    // Construct the request according to the virtio-blk specification.

    dev->requests->sector = sector;
    dev->requests->type = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    if (is_write)
        memcpy_s(dev->requests->data, sizeof dev->requests->data, buf, SECTOR_SIZE);

    // Construct the virtqueue descriptors (using 3 descriptors).
    struct virtio_virtq *vq = dev->virtio.queue;
    vq->descs[0].addr = (paddr_t)dev->requests;
    vq->descs[0].len = sizeof(uint32_t) * 2 + sizeof(uint64_t);
    vq->descs[0].flags = VIRTQ_DESC_F_NEXT;
    vq->descs[0].next = 1;

    vq->descs[1].addr = (paddr_t)dev->requests + offsetof(struct virtio_blk_req, data);
    vq->descs[1].len = SECTOR_SIZE;
    vq->descs[1].flags = VIRTQ_DESC_F_NEXT | (is_write ? 0 : VIRTQ_DESC_F_WRITE);
    vq->descs[1].next = 2;

    vq->descs[2].addr = (paddr_t)dev->requests + offsetof(struct virtio_blk_req, status);
    vq->descs[2].len = sizeof(uint8_t);
    vq->descs[2].flags = VIRTQ_DESC_F_WRITE;

    // Notify the device that there is a new request.
    virtq_kick(dev->virtio.base_addr, vq, 0);

    // Wait until the device finishes processing.
    while (virtq_is_busy(vq))
        ;

    // virtio-blk: If a non-zero value is returned, it's an error.
    if (dev->requests->status != 0) {
        kprintf("virtio: warn: failed to read/write sector=%d status=%d\n", sector, dev->requests->status);
        return;
    }

    // For read operations, copy the data into the buffer.
    if (!is_write)
        memcpy_s(buf, SECTOR_SIZE, dev->requests->data, SECTOR_SIZE);
}

#pragma once

#include <inheritance.h>
#include <io.h>
#include <stdint.h>

#ifdef FAT_DEBUG
#include <kernel.h>
#define FAT_DBG(...)   KDBG("FAT-CORE", __VA_ARGS__)
#define FAT12_DBG(...) KDBG("FAT12-CORE", __VA_ARGS__)
#define FAT16_DBG(...) KDBG("FAT16-CORE", __VA_ARGS__)
#else
#define FAT_DBG(...)
#define FAT12_DBG(...)
#define FAT16_DBG(...)
#endif

struct fat_filesystem {
    INHERITS(struct filesystem);
    size_t bytes_per_cluster, relative_first_data_sector, bytes_per_sector;
};

struct fat_file {
    INHERITS(struct file);
    uint32_t start_cluster;
};

bool fat_init(const struct block_device *dev, struct block *base);

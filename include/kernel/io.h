#pragma once

#include <stddef.h>

#include <inheritance.h>

#include <io.h>

#define SECTOR_SIZE         512
#define MAX_FILENAME_LENGTH 64

#ifdef IO_DEBUG
#include <kernel.h>
#define IO_DBG(...) KDBG("IO-CORE", __VA_ARGS__)
#else
#define IO_DBG(...)
#endif

struct filesystem {
    const struct filesystem *next;
    const struct block_device *device;
    const char *type_name;
    uint32_t base_sector;
    size_t (*read_file)(struct filesystem *, void *restrict, char (*name)[MAX_FILENAME_LENGTH]);
};
extern inline void add_filesystem(struct filesystem *);

enum FilesystemEntryType : uint8_t {
    FS_ENTRY_FILE,
    FS_ENTRY_DIR,
};

struct fs_entry {
    struct fs_entry *next;             // Next entry in the filesystem linked list.
    struct filesystem *filesystem;     // Filesystem the file originated from.
    char (*name)[MAX_FILENAME_LENGTH]; // File name
    enum FilesystemEntryType type;
} __attribute__((__packed__));

struct directory {
    INHERITS(struct fs_entry);
} __attribute__((__packed__));

struct file {
    INHERITS(struct fs_entry);
    size_t size; // File size
} __attribute__((__packed__));

extern inline void add_fs_entry(struct fs_entry *);

extern struct fs_entry *files_head;

struct device {
    struct device *next;
};

struct block_device {
    INHERITS(struct device);
    char *id;
    size_t (*read_block)(const struct block_device *dev, void *restrict buffer, size_t start_block, size_t num_blocks);
};

struct block {
    uint8_t data[SECTOR_SIZE];
    uint32_t block_number;
};

extern struct block_device *block_device_chain_head;
extern inline void add_block_device(struct block_device *);
void fs_init(struct block_device *dev);
static inline void fs_flush(struct block_device *dev);
struct file *fs_lookup(const char *);

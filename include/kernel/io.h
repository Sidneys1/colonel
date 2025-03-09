#pragma once

#include <stddef.h>

#include <inheritance.h>

#include <io.h>

#define SECTOR_SIZE 512

struct file {
    struct file *next;      // Indicates if this file entry is in use
    char name[56];   // File name
    // char data[29108]; // File content
    size_t size;      // File size
};
extern inline void add_file(struct file*);

extern struct file *files_head;

struct device {
    struct device *next;
};

struct block_device {
    INHERITS(struct device);
    char* id;
    size_t (*read_block)(const struct block_device*, void*restrict, size_t, size_t);
};


struct block {
    uint8_t data[SECTOR_SIZE];
    uint32_t block_number;
};

extern struct block_device *block_device_chain_head;
extern inline void add_block_device(struct block_device*);
void fs_init(struct block_device *dev);
static inline void fs_flush(struct block_device *dev);
struct file *fs_lookup(const char *);

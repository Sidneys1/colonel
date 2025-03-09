#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <common.h>

#include <kernel.h>
#include <memory/slab_allocator.h>
#include <io.h>
#include <drivers/filesystems/ustar.h>


static inline int oct2int(char *oct, int len) {
    int dec = 0;
    for (int i = 0; i < len; i++) {
        if (oct[i] < '0' || oct[i] > '7')
            break;

        dec = dec * 8 + (oct[i] - '0');
    }
    return dec;
}

bool ustar_init(struct block_device *dev, struct block *block) {
    static size_t ustar_number = 0;
    size_t num = ustar_number++;
    size_t off = 0;
    uint32_t start = block->block_number;
    struct block nblock;
    do {
        if (off != 0) {
            nblock.block_number = start + off;
            dev->read_block(dev, nblock.data, nblock.block_number, 1);
            block = &nblock;
        }

        struct tar_header *header = (struct tar_header *)(block->data);
        if (header->name[0] == '\0')
            break;

        if (strncmp(header->magic, "ustar", 6) != 0)
            PANIC("invalid tar header: magic=\"%S\"", header->magic);

        int filesz = oct2int(header->size, sizeof(header->size));

        struct file *file = slab_malloc(struct file);
        // char *path
        strncpy_s(file->name, sizeof file->name, header->name, sizeof header->name);
        snprintf(file->name, sizeof(file->name), "ustar%zu:/%S", num, header->name);
        // if ((unsigned int)filesz > sizeof file->data)
        //     PANIC("Cannot load file `%S`, because it is larger than the available buffer (%d vs %d)!\n", file->name,
        //           filesz, sizeof file->data);
        // memcpy_s(file->data, sizeof file->data, header->data, filesz);
        file->size = filesz;
        add_file(file);

        off += align_up(sizeof(struct tar_header) + filesz, SECTOR_SIZE) / SECTOR_SIZE;
    } while (true);
}

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <common.h>

#include <drivers/filesystems/ustar.h>
#include <io.h>
#include <kernel.h>
#include <memory/slab_allocator.h>

static inline int oct2int(char *oct, int len) {
    int dec = 0;
    for (int i = 0; i < len; i++) {
        if (oct[i] < '0' || oct[i] > '7')
            break;

        dec = dec * 8 + (oct[i] - '0');
    }
    return dec;
}

size_t read_ustar_file(struct filesystem *fs, void *restrict buffer, char (*path)[MAX_FILENAME_LENGTH]) {
    // struct ustar_filesystem* ustarfs = (struct ustar_filesystem*)fs;
    struct block block;
    block.block_number = 0;
    fs->device->read_block(fs->device, block.data, block.block_number, 1);

    size_t off = 0;
    do {
        block.block_number = off;
        if (fs->device->read_block(fs->device, block.data, block.block_number, 1) == 0) {
            PANIC("Got to end of disk and did not find file...\n");
            return 0;
        }

        struct tar_header *header = (struct tar_header *)(block.data);
        if (header->name[0] == '\0')
            break;

        if (strncmp(header->magic, "ustar", 6) != 0)
            PANIC("invalid tar header: magic=\"%S\"", header->magic);

        int filesz = oct2int(header->size, sizeof(header->size));

        const char *path_part =
            strchr((const_string){.head = (char *)path, .tail = (char *)path + MAX_FILENAME_LENGTH}, '/');
        if (path_part == NULL)
            PANIC("Could not find `/` in path...\n");
        if (strncmp(path_part + 1, header->name, strnlen_s(header->name, sizeof(header->name))) == 0) {
            // PANIC("Found file: `%S` == `%S`!\n", path_part + 1, header->name);

            const size_t num_whole_blocks = filesz / SECTOR_SIZE;
            // Read whole blocks...
            fs->device->read_block(fs->device, buffer, block.block_number + 1, num_whole_blocks);
            const size_t rem = filesz % SECTOR_SIZE;
            if (rem) {
                // Read remainder...
                fs->device->read_block(fs->device, block.data, block.block_number + 1 + num_whole_blocks, 1);
                memcpy_s(buffer + (num_whole_blocks * SECTOR_SIZE), rem, block.data, rem);
            }
            return filesz;
        }
        // printf("Didn't find file: `%S` != `%S`!\n", path_part + 1, header->name);
        off += align_up(sizeof(struct tar_header) + filesz, SECTOR_SIZE) / SECTOR_SIZE;
    } while (true);

    PANIC("read_ustar_file is not implemented!\n");
}

bool ustar_init(struct block_device *dev, struct block *block) {
    struct ustar_filesystem *fs = slab_malloc(struct ustar_filesystem);
    fs->super.type_name = "USTAR";
    fs->super.device = dev;
    fs->super.read_file = read_ustar_file;
    add_filesystem(SUPER(*fs));

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

        struct ustar_file *file = slab_malloc(struct ustar_file);
        file->super.super.filesystem = SUPER(*fs);

        char *buffer = _slab_malloc(MAX_FILENAME_LENGTH);
        file->super.super.name = (char (*)[MAX_FILENAME_LENGTH])buffer;
        // char *path
        // strncpy_s(buffer, sizeof *file->name, header->name, sizeof header->name);
        snprintf(buffer, sizeof(*file->super.super.name), "ustar%zu:/%S", num, header->name);
        // if ((unsigned int)filesz > sizeof file->data)
        //     PANIC("Cannot load file `%S`, because it is larger than the available buffer (%d vs %d)!\n", file->name,
        //           filesz, sizeof file->data);
        // memcpy_s(file->data, sizeof file->data, header->data, filesz);
        file->super.size = filesz;
        add_fs_entry(SUPER(*SUPER(*file)));

        off += align_up(sizeof(struct tar_header) + filesz, SECTOR_SIZE) / SECTOR_SIZE;
    } while (true);
}

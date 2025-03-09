#include <kernel.h>
#include <drivers/filesystems/ustar.h>
#include <drivers/filesystems/fat.h>

#include <io.h>

struct file *files_head = NULL, *files_tail = NULL;

struct block_device *block_device_chain_head = NULL, *block_device_chain_tail = NULL;

struct mbr_parttable {
    uint8_t bootable;
    struct { uint8_t head; uint8_t sector : 6; uint16_t cylinder : 10; } __attribute__((__packed__)) start;
    enum MBR_SYSTEM_ID : uint8_t {
        MBR_SYS_FREE = 0x0,
        MBR_SYS_FAT12_PRIMARY = 0x1,
        MBR_SYS_FAT16_PRIMARY = 0x4,
        MBR_SYS_EXTENDED_PARTITION_CHS,
        MBR_SYS_FAT,
        MBR_SYS_NTFS = 0x7, MBR_SYS_EXFAT = 0x7,
        // MBR_SYS_LOGICAL_FAT,
        MBR_SYS_FAT32_CHS = 0xb,
        MBR_SYS_FAT32_LBA,
        // MBR_SYS_FAT16B_LBA = 0xe,
        MBR_SYS_EXTENDED_PARTITION_LBA,
        // MBR_SYS_LOGICAL_FAT_2_ELECTRIC_BOOGALOO = 0x11,
        // MBR_SYS_NEC_LOGICAL_SECTORED_FAT = 0x24,
        MBR_SYS_GPT = 0xee,
    } system_id;
    struct { uint8_t head; uint8_t sector : 6; uint16_t cylinder : 10; } __attribute__((__packed__)) end;
    uint32_t relative_sector, sector_count;
} __attribute__((__packed__));

struct gpt_head {
    char signature[8];
    uint32_t revision, header_size, checksum, reserved;
    uint64_t this_lba, alt_lba, first_useable_block, last_useable_block;
    uint8_t guid[16];
    uint64_t starting_lba_of_guid_partition_entry_array;
    uint32_t number_of_partition_entries, size_of_partition_entry, partition_entries_crc32;
};

struct gpt_part {
    uint8_t type_guid[16], guid[16];
    uint64_t starting_lba, ending_lba, attributes;
    char partition_name[72];
};

void sniff(const struct block_device *dev, struct block *base) {
    // const uint8_t *d = (uint8_t*)base->data;
    // printf("sniff %p:\n", d);
    if (base->data[0] == 0xeb && base->data[2] == 0x90) {
        fat_init(dev, base);
    } else {
        printf(ANSI_RED "Partition at sector #%u is not any known format...\n", base->block_number);
    }
}

void gpt_part_init(const struct block_device *dev, struct block *base) {
    struct gpt_part *part = (struct gpt_part*)base->data;
    printf("GPT Partition:\n"
           "\t`");
    for (size_t i = 0; i < sizeof(part->partition_name) && part->partition_name[i] != '\0'; i+=2)
        putchar(part->partition_name[i]);
    printf("`\n\tType: ");
    for (size_t i = 0; i < 4; i++)
        printf("%08X", *((uint32_t*)(&part->type_guid) + i));
    putchar('\n');
    // printf("LBA0: %p\n", lba0);
    // printf("First LBA: %u\n", (uint32_t)part->starting_lba);
    // printf("Sniffing at %p\n", lba0 + (paddr_t)((SECTOR_SIZE * (paddr_t)part->starting_lba)));
    struct block block = {.block_number = part->starting_lba};
    dev->read_block(dev, block.data, block.block_number, 1);
    sniff(dev, &block);
}

bool gpt_init(const struct block_device *dev, struct block *base) {
    struct gpt_head *gpt = (struct gpt_head*)base->data;
    const_string sig = {.head=gpt->signature, .tail=gpt->signature+sizeof(gpt->signature)};
    if (strstr(sig, CSTR("EFI PART")).head == NULL) {
        // No signature match, this isn't a proper GPT partition header.
        return false;
    }
    printf("GPT Signature: `%s`\n"
           "\tRevision: %#x\n"
           "\tHeader size: %u\n"
           "\tChecksum: %08X\n"
        //    "\tThis/alternate LBA: %llu/%llu\n"
        //    "\tFirst/last useable blocks: %llu/%llu\n"
        //    "\tLBA of partition table entries: %llu\n"
           "\tNumber of partition table entries: %u\n"
           "\tPartition table entry size: %uB\n"
           "\tPartition table entries checksum: %08X\n",
           sig,
           gpt->revision,
           gpt->header_size,
           gpt->checksum,
        //    gpt->this_lba, gpt->alt_lba,
        //    gpt->first_useable_block, gpt->last_useable_block,
        //    gpt->starting_lba_of_guid_partition_entry_array,
           gpt->number_of_partition_entries,
           gpt->size_of_partition_entry,
           gpt->partition_entries_crc32);
    uint64_t lba = gpt->starting_lba_of_guid_partition_entry_array /*- gpt->this_lba*/;
    struct block block = {.block_number = lba};
    dev->read_block(dev, block.data, block.block_number, 1);
    gpt_part_init(dev, &block);
    return true;
}

void mbr_init(const struct block_device *dev, struct block *params) {
    const struct mbr_parttable *mbr = (struct mbr_parttable*)&params->data[446];
    for (size_t i = 0; i < 4; i++) {
        if (mbr[i].system_id == MBR_SYS_FREE) {
            printf(ANSI_ORANGE "MBR Partition[%zu]: Unused.\n", i);
            continue;
        }
        printf("MBR Partition[%zu]:"
                "\tBootable: %s\n"
                "\tStarting head/sector/cylinder: %#hhx/%#hhx/%#hx\n"
                "\tSystem type: %#hhx\n"
                "\tEnding head/sector/cylinder: %#hhx/%#hhx/%#hx\n"
                "\tRelative starting block: %u\n"
                "\tSector blocks: %u\n",
                i,
                mbr[i].bootable ? CSTR("Yes") : CSTR("No"),
                mbr[i].start.head, mbr[i].start.sector, mbr[i].start.cylinder,
                mbr[i].system_id,
                mbr[i].end.head, mbr[i].end.sector, mbr[i].end.cylinder,
                mbr[i].relative_sector,
                mbr[i].sector_count);

        struct block block = {.block_number = mbr[i].relative_sector};
        dev->read_block(dev, block.data, block.block_number, 1);
        switch (mbr[i].system_id) {
            case MBR_SYS_FAT12_PRIMARY:
            case MBR_SYS_FAT16_PRIMARY:
            case MBR_SYS_FAT32_CHS: {
                // Disk *appears* to be some FAT variant.
                if (fat_init(dev, &block))
                    // Disk is actually FAT!
                    continue;
            } break;
            case MBR_SYS_GPT: {
                // Disk *appears* to be GPT...
                if (gpt_init(dev, &block))
                    // Disk is actually GPT, there won't be any other MBR partitions.
                    return;
            } break;
            default: {
                kprintf(ANSI_ORANGE "Unknown/unsupported MBR partition type: %#hhX.\n", mbr[i].system_id);
            } break;
        }

        // Partition wasn't GPT... let's sniff?
        sniff(dev, &block);
    }
}


void add_file(struct file *file) {
    if (files_head == NULL) {
        files_head = files_tail = file;
        return;
    }
    files_tail = (files_tail->next = file);
    file->next = NULL;
}

void add_block_device(struct block_device *dev) {
    if (block_device_chain_head == NULL) {
        block_device_chain_head = block_device_chain_tail = dev;
        return;
    }
    block_device_chain_tail->super.next = (block_device_chain_tail->super.next = SUPER(*dev));
    dev->super.next = NULL;
}

void fs_init(struct block_device *dev) {
    if (dev->id != NULL)
        kprintf("Initializing block device `%S`...\n", dev->id);
    else
        kprintf("Initializing anonymous block device...\n");

    // uint8_t disk[SECTOR_SIZE];
    struct block lba0 = {.block_number = 0};
    // for (unsigned sector = 0; sector < (sizeof(lba0.data) / SECTOR_SIZE) && sector < dev->sector_count; sector++)
    //     read_write_disk(dev->, &lba0.data[sector * SECTOR_SIZE], sector, false);
    dev->read_block(dev, lba0.data, 0, 1);

    if (lba0.data[0x1fe + 0] == 0x55 && lba0.data[0x1fe + 1] == 0xaa) {
        mbr_init(dev, &lba0);
    } else if (strncmp((const char*)&lba0.data[257], "ustar", 6) == 0) {
        printf("Ustar (TAR) formatted drive\n");
        ustar_init(dev, &lba0);
    } else if (lba0.data[0] == 0xeb && lba0.data[2] == 0x90) {
        fat_init(dev, &lba0);
    } // else {
    //     printf("%hhx %hhx %hhx %hhx\n", disk[0], disk[1], disk[2], disk[3]);
    //     PANIC("hmmm");
    // }

    ;
    for (struct file *c = files_head; c != NULL; c = c->next) {
        // if (!files[i].in_use) continue;
        kprintf("file: %S, size=%d\n", c->name, c->size);
    }
}

void fs_flush(struct block_device *dev) {
    // Copy all file contents into `disk` buffer.
    // memset_s(disk, sizeof disk, 0, sizeof disk);
    // unsigned off = 0;
    // for (int file_i = 0; file_i < FILES_MAX; file_i++) {
    //     struct file *file = &files[file_i];
    //     if (!file->in_use)
    //         continue;

    //     struct tar_header *header = (struct tar_header *)&disk[off];
    //     memset_s(header, sizeof *header, 0, sizeof *header);
    //     strncpy_s(header->name, sizeof header->name, file->name, sizeof file->name);
    //     strncpy_s(header->mode, sizeof header->mode, "000644", sizeof "000644");
    //     strncpy_s(header->magic, sizeof header->magic, "ustar", sizeof "ustar");
    //     strncpy_s(header->version, sizeof header->version, "00", sizeof "00");
    //     header->type = '0';

    //     // Turn the file size into an octal string.
    //     int filesz = file->size;
    //     for (int i = sizeof(header->size); i > 0; i--) {
    //         header->size[i - 1] = (filesz % 8) + '0';
    //         filesz /= 8;
    //     }

    //     // Calculate the checksum.
    //     int checksum = ' ' * sizeof(header->checksum);
    //     for (unsigned i = 0; i < sizeof(struct tar_header); i++)
    //         checksum += (unsigned char)disk[off + i];

    //     for (int i = 5; i >= 0; i--) {
    //         header->checksum[i] = (checksum % 8) + '0';
    //         checksum /= 8;
    //     }

    //     // Copy file data.
    //     memcpy_s(header->data, (sizeof disk) - off, file->data, file->size);
    //     off += align_up(sizeof(struct tar_header) + file->size, SECTOR_SIZE);
    // }

    // // Write `disk` buffer into the virtio-blk.
    // for (unsigned sector = 0; sector < (sizeof(disk) / SECTOR_SIZE) && sector < dev->sector_count; sector++)
    //     read_write_disk(dev, &disk[sector * SECTOR_SIZE], sector, true);

    // kprintf("wrote %d bytes to disk\n", sizeof(disk));
}

struct file *fs_lookup(const char *filename) {
    for (struct file *file = files_head; file != NULL; file = file->next) {
        if (!strncmp(file->name, filename, sizeof file->name))
            return file;
    }

    return NULL;
}

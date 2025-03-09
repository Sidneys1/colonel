#include <stddef.h>
#include <stdio.h>

#include <kernel.h>
#include <io.h>
#include <memory/slab_allocator.h>

#include <drivers/filesystems/fat.h>

struct __attribute__((__packed__)) fat {
    uint8_t magic[3];
    char version[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t number_of_fats;
    uint16_t number_of_root_directory_entries, logical_sector_count;
    uint8_t media_descriptor;
    uint16_t sectors_per_fat, sectors_per_track, number_of_heads_or_sides;
    uint32_t number_of_hidden_sectors, large_sector_count;
};

struct __attribute__((__packed__)) fat_12_16 {
    struct fat fat;
    uint8_t drive_number, flags, signature;
    uint32_t serial;
    char label[11], system_identifier[8];
    uint8_t boot_code[448];
    uint16_t bootable_signature;
};

struct cluster {
} __attribute__((__packed__));

struct FATable {
    uint16_t media_type, partition_state;
    struct cluster clusters[];
}__attribute__((__packed__));

struct fat16_time {
  union {
    struct {
      uint8_t hours : 5, minutes : 6, seconds : 5;
    } __attribute__((__packed__));
    uint16_t raw;
  };
} __attribute__((__packed__));

struct fat16_date {
  union {
    struct {
      uint8_t day : 5, month : 4, year : 7;
    } __attribute__((__packed__));
    uint16_t raw;
  };
} __attribute__((__packed__));

struct directory {
    union { uint8_t marker;
            struct {char name[8], ext[3];};
            char name8_3[11]; };
    enum directory_attribute : uint8_t { NONE=0, READ_ONLY, HIDDEN, SYSTEM=0x04, VOLUME_ID=0x08, DIRECTORY=0x10, ARCHIVE=0x20, LFN=READ_ONLY|HIDDEN|SYSTEM|VOLUME_ID } attributes;
    uint8_t reserved, creation_time_microseconds;
    struct fat16_time creation_time;
    struct fat16_date creation_date;
    struct fat16_date last_accessed_date;
    uint16_t cluster_high;
    struct fat16_time last_mod_time;
    struct fat16_date last_mod_date;
    uint16_t cluster_low;
    uint32_t file_size;
}__attribute__((__packed__));

typedef uint16_t wchar_t;

struct long_filename {
    uint8_t marker;
    wchar_t name[5];
    enum directory_attribute attribute;
    uint8_t reserved, chk_short_name;
    wchar_t name2[6];
    uint16_t cluster;
    wchar_t name3[2];
} __attribute__((__packed__));



bool fat_init(const struct block_device *dev, struct block *base) {
    static size_t fat_no = 0;
    const struct fat *fat = (struct fat*)base->data;
    kprintf(ANSI_GREEN "\n[FAT] Found FAT-formatted disk at sector #%u.\n"
            "\tVersion: `%s`.\n"
            "\tBytes per sector: %hu\n"
            "\tSectors per cluster: %hhu\n"
            "\tReserved sectors: %hu\n"
            "\tFAT count: %hhu\n"
            "\tRoot directory entries: %hu\n"
            "\tLogical sectors: %hu\n"
            "\tMedia descriptor: %#hhx\n"
            "\tSectors per FAT: %hu\n"
            "\tSectors per track: %hu\n"
            "\tHeads/sides: %hu\n"
            "\tHidden sectors: %u\n"
            "\tLarge sectors: %u\n",
            base->block_number,
            (const_string){.head=fat->version, .tail=fat->version+sizeof(fat->version)},
            fat->bytes_per_sector,
            fat->sectors_per_cluster,
            fat->reserved_sectors,
            fat->number_of_fats,
            fat->number_of_root_directory_entries,
            fat->logical_sector_count,
            fat->media_descriptor,
            fat->sectors_per_fat,
            fat->sectors_per_track,
            fat->number_of_heads_or_sides,
            fat->number_of_hidden_sectors,
            fat->large_sector_count
    );

    const unsigned int root_dir_sectors = ((fat->number_of_root_directory_entries * 32) + (fat->bytes_per_sector - 1)) / fat->bytes_per_sector;
    const unsigned int first_data_sector = fat->reserved_sectors + (fat->sectors_per_fat * fat->number_of_fats) + root_dir_sectors;
    const unsigned int data_sectors = fat->logical_sector_count - (fat->reserved_sectors + (fat->number_of_fats * fat->sectors_per_fat) + root_dir_sectors);
    const unsigned int total_clusters = data_sectors / fat->sectors_per_cluster;

    if (fat->bytes_per_sector == 0) {
        printf(ANSI_RED "Disk is exFAT, not FAT12!\n");
        return true;
    } else if (total_clusters < 4085) {
        printf(ANSI_GREEN "Disk is FAT12\n");
    } else if (total_clusters < 65525) {
        printf(ANSI_ORANGE "Disk is FAT16!\n");
    } else {
        printf(ANSI_RED "Disk is FAT32, not FAT12!\n");
        return true;
    }

    const struct fat_12_16 *fat2 = (struct fat_12_16*)fat;

    kprintf(ANSI_GREEN "\n[FAT12/16] FAT12/16-formatted disk.\n"
            "\tRoot directory sectors: %u\n"
            "\tFirst data sector: %u\n"
            "\tData sectors: %u\n"
            "\tTotal clusters: %u\n"
            "\tDrive number: %hhu\n"
            "\tFlags: %#hhx (%#hhb)\n"
            "\tSignature: %#hhx\n"
            "\tSerial: `%lX`\n"
            "\tLabel: `%s`\n"
            "\tSystem identifier: `%s`\n"
            "\tBootable signature: %#hx\n",
            root_dir_sectors,
            first_data_sector,
            data_sectors,
            total_clusters,
            fat2->drive_number,
            fat2->flags, fat2->flags,
            fat2->signature,
            fat2->serial,
            (const_string){.head=fat2->label, .tail=fat2->label+sizeof(fat2->label)},
            (const_string){.head=fat2->system_identifier, .tail=fat2->system_identifier+sizeof(fat2->system_identifier)},
            fat2->bootable_signature
    );


    const unsigned int first_root_dir_sector = first_data_sector - root_dir_sectors;
    // unsigned int first_sector_of_cluster = ((cluster - 2) * fat->sectors_per_cluster) + first_data_sector;
    kprintf(ANSI_GREEN "\tFirst root dir sector: %u (%u)\n", first_root_dir_sector, first_root_dir_sector * SECTOR_SIZE);

    if (total_clusters > 4085) return false;

    uint32_t active_cluster = 0;

#define fat_offset(active_cluster) (active_cluster + (active_cluster / 2))
#define fat_sector(x) (1 + (fat_offset(x) / SECTOR_SIZE))
#define ent_offset(x) (fat_offset(x) % SECTOR_SIZE)

    // unsigned char FAT_table[SECTOR_SIZE * 2]; // needs two in case we straddle a sector
    // unsigned int fat_offset = active_cluster + (active_cluster / 2);// multiply by 1.5
    // unsigned int fat_sector = 1 + (fat_offset / SECTOR_SIZE);
    // unsigned int ent_offset = fat_offset % SECTOR_SIZE;


    //at this point you need to read two sectors from disk starting at "fat_sector" into "FAT_table".

#define disc_off(x) (fat_sector(x) * SECTOR_SIZE + ent_offset(x))
#define _table_value(x) (*(unsigned short*)(data + ent_offset(x)))
#define table_value(x) ((x & 1) ? _table_value(x) >> 4 : _table_value(x) & 0xfff)

    uint32_t offset = disc_off(active_cluster);
    printf("Offset: %u (block #%u)\n", offset, offset / SECTOR_SIZE);

    uint8_t data[SECTOR_SIZE * 2];
    uint32_t data_sector = offset/SECTOR_SIZE;
    dev->read_block(dev, data, data_sector, 2);
    // unsigned short table_value = *(unsigned short*)&disk[fat_sector * SECTOR_SIZE + ent_offset];

    // table_value = (active_cluster & 1) ? table_value >> 4 : table_value & 0xfff;

    // printf(ANSI_CYAN "fat_offset=%u\nfat_sector=%u\nent_offset=%u\n", fat_offset(active_cluster), fat_sector(active_cluster), ent_offset(active_cluster));

    uint16_t tv = table_value(active_cluster);
    if ((tv & 0xf00) != 0xf00 || (tv & 0x0ff) != fat->media_descriptor) {
        kprintf(ANSI_RED "Cluster[0] at (%u) was not 0xf%02x (it was %#06hx)!\n", (void*)&fat->media_descriptor - (void*)fat, fat->media_descriptor, tv);
        return false;
    } //else
    //    kprintf(ANSI_GREEN "\tCluster[0]=%#hx\n", tv);

    active_cluster = 1;
    offset = disc_off(active_cluster);
    printf("Offset: %u (block #%u)\n", offset, offset / SECTOR_SIZE);
    if ((offset / SECTOR_SIZE) != data_sector)
        dev->read_block(dev, data, data_sector, 2);

    // printf(ANSI_CYAN "fat_offset=%u\nfat_sector=%u\nent_offset=%u\n", fat_offset(active_cluster), fat_sector(active_cluster), ent_offset(active_cluster));
    const  uint16_t file_end = tv = table_value(active_cluster);
    if (tv != 0xfff) {
        kprintf(ANSI_RED "Cluster[1] was not 0xfff (it was %#hx)!\n", tv);
        return false;
    } //else
    //    kprintf(ANSI_GREEN "\tCluster[1]=%#hx\n", tv);

    // for (active_cluster = 2; active_cluster < total_clusters; active_cluster++) {
        // printf(ANSI_CYAN "fat_offset=%u\nfat_sector=%u\nent_offset=%u\n", fat_offset(active_cluster), fat_sector(active_cluster), ent_offset(active_cluster));
        // tv = table_value(active_cluster);
        // if (tv != 0) {
        //     if (tv == file_end)
        //         kprintf(ANSI_GREEN "\tCluster[%u]=-file end-\n", active_cluster);
        //     else
        //         kprintf(ANSI_GREEN "\tCluster[%u]=%#hu\n", active_cluster, tv);
        // }
    // }

    const_string lFilename[3] = { (const_string){.head=NULL, .tail=NULL}, (const_string){.head=NULL, .tail=NULL}, (const_string){.head=NULL, .tail=NULL}};
    // const struct directory *d = (struct directory*)(base + first_root_dir_sector * SECTOR_SIZE);
    // offset = first_root_dir_sector;

    const uint32_t bytes_per_cluster = (fat->sectors_per_cluster * fat->bytes_per_sector);
    kprintf("Each cluster is %u bytes. Data starts at %#p\n", bytes_per_cluster, first_data_sector * fat->bytes_per_sector);

    printf("Root directory sectors: %u\n", fat->number_of_root_directory_entries * sizeof(struct directory) / SECTOR_SIZE);

    size_t this_drive = fat_no++;
    const struct directory *d = (struct directory*)data;
    // return true;
    for (int i = 0; i < fat->number_of_root_directory_entries; i++) {
        uint32_t sector_i = i * sizeof(struct directory) / SECTOR_SIZE;
        if (i % (sizeof(data) / sizeof(struct directory)) == 0) {
            kprintf("Copying in blocks %zu-%zu...\n", first_root_dir_sector + sector_i, first_root_dir_sector + sector_i + 1);
            dev->read_block(dev, data, first_root_dir_sector + sector_i, 2);
        }
        const struct directory *this_entry = &d[i % (sizeof(data) / sizeof(struct directory))];
        if (this_entry->marker == 0xe5) {
            // printf("-unused entry-\n");
            continue;
        }
        if (this_entry->marker == 0x00) break;
        if (this_entry->attributes == LFN) {
            const struct long_filename *fname = (struct long_filename*)&this_entry;
            lFilename[0] = (const_string){.head=(char*)fname->name, .tail=(char*)fname->name+(sizeof(fname->name))};
            lFilename[1] = (const_string){.head=(char*)fname->name2, .tail=(char*)fname->name2+(sizeof(fname->name2))};
            lFilename[2] = (const_string){.head=(char*)fname->name3, .tail=(char*)fname->name3+(sizeof(fname->name3))};
            // kprintf("Long filename: `%s%s%s`\n",
            //     ,
            //     (const_string),
            //     (const_string)
            // );

            continue;
        }
        const_string fname = {.head=this_entry->name, .tail=this_entry->ext};
        const_string fext = {.head=this_entry->ext, .tail=this_entry->ext+sizeof(this_entry->ext)};
        char *end = strchr(fname, ' ');
        if (end != NULL) fname.tail = end;
        if (this_entry->attributes == VOLUME_ID) {
            kprintf(ANSI_ORANGE "\nLABEL: `%s.%s`\n", fname, (const_string){.head=this_entry->ext, .tail=this_entry->ext+sizeof(this_entry->ext)});
            continue;
        }
        kprintf(ANSI_CYAN "\nFile: `%s.%s` %c%s%s%s%c\n"
                "\tAttributes: %#hhx\n"
                "\tCreated: %u-%02hhu-%02hhu %02hhu:%02hhu:%02hhu.%03hhu\n"
                "\tModified: %u-%02hhu-%02hhu %02hhu:%02hhu:%02hhu\n"
                "\tAccessed: %u-%02hhu-%02hhu\n"
                "\tCluster: H:%hu L:%hu\n"
                "\tSize: %u\n",
                fname, (const_string){.head=this_entry->ext, .tail=this_entry->ext+sizeof(this_entry->ext)},
                lFilename[0].head == NULL ? ' ' : '(', lFilename[0].head == NULL ? CSTR("") : lFilename[0], lFilename[0].head == NULL ? CSTR("") : lFilename[1], lFilename[0].head == NULL ? CSTR("") : lFilename[2], lFilename[0].head == NULL ? ' ' : ')',
                this_entry->attributes,
                1980+this_entry->creation_date.year, this_entry->creation_date.month, this_entry->creation_date.day, this_entry->creation_time.hours, this_entry->creation_time.minutes, this_entry->creation_time.seconds, this_entry->creation_time_microseconds,
                1980+this_entry->last_mod_date.year, this_entry->last_mod_date.month, this_entry->last_mod_date.day, this_entry->last_mod_time.hours, this_entry->last_mod_time.minutes, this_entry->last_mod_time.seconds,
                1980+this_entry->last_accessed_date.year, this_entry->last_accessed_date.month, this_entry->last_accessed_date.day,
                this_entry->cluster_high, this_entry->cluster_low,
                this_entry->file_size);

        uint32_t rem = this_entry->file_size % bytes_per_cluster;
        const uint32_t expected_clusters = this_entry->file_size / bytes_per_cluster + (rem ? 1 : 0);
        printf(ANSI_CYAN "\tFile should be %u clusters (last cluster is only %u bytes). ", expected_clusters, rem);

        // const uint32_t start_cluster = ((uint32_t)this_entry->cluster_high << 16) | this_entry->cluster_low;
        // uint32_t last_cluster = start_cluster;
        // active_cluster = table_value(start_cluster);
        // bool sequential = active_cluster == (last_cluster + 1);
        // size_t n = 1;
        // for (; active_cluster != file_end; n++, last_cluster = active_cluster, active_cluster = table_value(active_cluster))// {
        //     sequential &= active_cluster == (last_cluster + 1);
        //     // printf("%u ", active_cluster);
        // // }
        // printf(n == expected_clusters ? (ANSI_GREEN "Verified. ") : (ANSI_RED "Following the cluster chain, we found %zu clusters intstead!"), n);
        // printf(sequential ? (ANSI_CYAN "Clusters were sequential.\n") : (ANSI_ORANGE "Clusters weren't sequential.\n"));

        // uint8_t *data = (uint8_t*)fat + (first_data_sector * fat->bytes_per_sector) + ((start_cluster - 2) * bytes_per_cluster);
        // kprintf("Data: %hhx %hhx %hhx %hhx\n", data[0], data[1], data[2], data[3]);
        // if (sequential)
        //     kprintf("Data runs from %#010x to %#010x.\n", (uint32_t)(first_data_sector * fat->bytes_per_sector) + ((start_cluster - 2) * bytes_per_cluster), (uint32_t)(first_data_sector * fat->bytes_per_sector) + ((start_cluster - 2) * bytes_per_cluster) + this_entry->file_size);

        struct file * file = slab_malloc(struct file);

        file->name[0] = '/';
        //if (lFilename[0].head == NULL) {
            const_string find = strstr(fname, CSTR(" "));
            const_string sname = {.head=fname.head, .tail=find.tail == NULL ? fname.tail : (fname.head + (find.tail - find.head))};
            // strcpy((string){.head=file->name + 1, .tail=file->name+sizeof(file->name)}, sname);
            // file->name[sname.tail - sname.head +1] = '.';

            find = strstr(fext, CSTR(" "));
            const_string sext = {.head=fext.head, .tail=find.tail == NULL ? fext.tail : (fext.head + (find.tail - find.head))};
            // strcpy((string){.head=file->name + (sname.tail - sname.head + 2), .tail=file->name+sizeof(file->name)}, sext);
            snprintf(file->name, sizeof(file->name), "fat%zu:/%s.%s", this_drive, sname, sext);
        /*} else {
            size_t name_i = 1, copy_i = 0;
            for (; copy_i < (size_t)(lFilename[0].tail - lFilename[0].head) && lFilename[0].head[copy_i] != '\xff'; copy_i+=2)
                file->name[name_i++] = lFilename[0].head[copy_i];

            if (copy_i == (size_t)(lFilename[0].tail - lFilename[0].head)) {
                copy_i = 0;
                for (; copy_i < (size_t)(lFilename[1].tail - lFilename[1].head) && lFilename[1].head[copy_i] != '\xff'; copy_i+=2)
                    file->name[name_i++] = lFilename[1].head[copy_i];

                if (copy_i == (size_t)(lFilename[1].tail - lFilename[1].head)) {
                    copy_i = 0;
                    for (; copy_i < (size_t)(lFilename[2].tail - lFilename[2].head) && lFilename[2].head[copy_i] != '\xff'; copy_i+=2)
                        file->name[name_i++] = lFilename[2].head[copy_i];
                }
            }
            if (name_i < sizeof(file->name))
                file->name[name_i] = '\0';
        }*/

        file->size = this_entry->file_size;
        add_file(file);
        // if (this_entry->file_size > sizeof file->data)
        //     PANIC("Cannot load file `%S`, because it is larger than the available buffer (%u vs %zu)!\n", file->name, this_entry->file_size, sizeof file->data);
        // if (!sequential)
        //     PANIC("Don't know how to memcpy fragmented files yet...\n");
        // memcpy_s(file->data, sizeof file->data, data, this_entry->file_size);

        if (lFilename[0].head != NULL)
            lFilename[0] = lFilename[1] = lFilename[2] = (const_string){NULL, NULL};
    }

    // PANIC("notdone\n");

    return true;
}

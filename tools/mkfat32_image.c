#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SECTOR_SIZE 512U
#define TOTAL_SECTORS 2048U
#define RESERVED_SECTORS 1U
#define FAT_COUNT 1U
#define FAT_SECTORS 16U
#define ROOT_CLUSTER 2U
#define SHELL_FIRST_CLUSTER 3U
#define EDIT_CLUSTER_COUNT 8U
#define IMAGE_SIZE (TOTAL_SECTORS * SECTOR_SIZE)
#define FAT32_EOC 0x0fffffffU

static void put_le16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put_le32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static long file_size(FILE *file) {
    long size;

    if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
        return -1;
    }

    size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        return -1;
    }

    return size;
}

static uint32_t clusters_for_size(uint32_t size) {
    return (size + SECTOR_SIZE - 1U) / SECTOR_SIZE;
}

static uint32_t data_start_sector(void) {
    return RESERVED_SECTORS + FAT_COUNT * FAT_SECTORS;
}

static void write_boot_sector(uint8_t *image) {
    uint8_t *boot = image;

    boot[0] = 0xeb;
    boot[1] = 0x58;
    boot[2] = 0x90;
    boot[3] = 'K';
    boot[4] = 'O';
    boot[5] = 'L';
    boot[6] = 'I';
    boot[7] = 'B';
    boot[8] = 'R';
    boot[9] = 'I';
    boot[10] = ' ';
    put_le16(&boot[11], SECTOR_SIZE);
    boot[13] = 1;
    put_le16(&boot[14], RESERVED_SECTORS);
    boot[16] = FAT_COUNT;
    put_le16(&boot[17], 0);
    put_le16(&boot[19], 0);
    boot[21] = 0xf8;
    put_le16(&boot[22], 0);
    put_le16(&boot[24], 1);
    put_le16(&boot[26], 1);
    put_le32(&boot[28], 0);
    put_le32(&boot[32], TOTAL_SECTORS);
    put_le32(&boot[36], FAT_SECTORS);
    put_le16(&boot[40], 0);
    put_le16(&boot[42], 0);
    put_le32(&boot[44], ROOT_CLUSTER);
    put_le16(&boot[48], 0);
    put_le16(&boot[50], 0);
    boot[64] = 0x80;
    boot[66] = 0x29;
    put_le32(&boot[67], 0x4b41524dU);
    boot[71] = 'K';
    boot[72] = 'O';
    boot[73] = 'L';
    boot[74] = 'I';
    boot[75] = 'B';
    boot[76] = 'R';
    boot[77] = 'I';
    boot[78] = 'A';
    boot[79] = 'R';
    boot[80] = 'M';
    boot[82] = 'F';
    boot[83] = 'A';
    boot[84] = 'T';
    boot[85] = '3';
    boot[86] = '2';
    boot[510] = 0x55;
    boot[511] = 0xaa;
}

static void write_cluster_chain(uint8_t *fat, uint32_t first_cluster,
                                uint32_t cluster_count) {
    for (uint32_t i = 0; i < cluster_count; i++) {
        uint32_t cluster = first_cluster + i;
        uint32_t next = i + 1U == cluster_count ? FAT32_EOC : cluster + 1U;

        put_le32(&fat[cluster * 4U], next);
    }
}

static void write_fat(uint8_t *image, uint32_t shell_cluster_count,
                      uint32_t edit_first_cluster,
                      uint32_t hello_cluster_count,
                      uint32_t hello_first_cluster) {
    uint8_t *fat = &image[RESERVED_SECTORS * SECTOR_SIZE];

    put_le32(&fat[0], 0x0ffffff8U);
    put_le32(&fat[4], FAT32_EOC);
    put_le32(&fat[ROOT_CLUSTER * 4U], FAT32_EOC);
    write_cluster_chain(fat, SHELL_FIRST_CLUSTER, shell_cluster_count);
    write_cluster_chain(fat, edit_first_cluster, EDIT_CLUSTER_COUNT);
    if (hello_cluster_count != 0U) {
        write_cluster_chain(fat, hello_first_cluster, hello_cluster_count);
    }
}

static void write_dir_entry(uint8_t *entry, const char name[11],
                            uint32_t first_cluster, uint32_t file_size_bytes) {
    for (uint32_t i = 0; i < 11U; i++) {
        entry[i] = (uint8_t)name[i];
    }

    entry[11] = 0x20;
    put_le16(&entry[20], 0);
    put_le16(&entry[26], (uint16_t)first_cluster);
    put_le32(&entry[28], file_size_bytes);
}

static void write_root_entries(uint8_t *image, uint32_t shell_size,
                               uint32_t edit_first_cluster,
                               uint32_t hello_size,
                               uint32_t hello_first_cluster) {
    uint8_t *root = &image[data_start_sector() * SECTOR_SIZE];
    const char shell_name[11] = {
        'S', 'H', 'E', 'L', 'L', ' ', ' ', ' ', 'B', 'I', 'N',
    };
    const char edit_name[11] = {
        'E', 'D', 'I', 'T', ' ', ' ', ' ', ' ', 'T', 'X', 'T',
    };
    const char hello_name[11] = {
        'H', 'E', 'L', 'L', 'O', ' ', ' ', ' ', 'K', 'L', 'I',
    };
    static const char edit_text[] = "ArmoniOS editable FAT32 file\n";

    write_dir_entry(root, shell_name, SHELL_FIRST_CLUSTER, shell_size);
    write_dir_entry(root + 32U, edit_name, edit_first_cluster,
                    sizeof(edit_text) - 1U);
    if (hello_size != 0U) {
        write_dir_entry(root + 64U, hello_name, hello_first_cluster,
                        hello_size);
    }
}

static int copy_payload(uint8_t *image, FILE *input, uint32_t size,
                        uint32_t first_cluster) {
    uint32_t lba = data_start_sector() + (first_cluster - ROOT_CLUSTER);
    uint8_t *dest = &image[lba * SECTOR_SIZE];

    return fread(dest, 1, size, input) == size ? 0 : -1;
}

static void write_edit_payload(uint8_t *image, uint32_t edit_first_cluster) {
    uint32_t lba = data_start_sector() +
                   (edit_first_cluster - ROOT_CLUSTER);
    uint8_t *dest = &image[lba * SECTOR_SIZE];
    static const char edit_text[] = "ArmoniOS editable FAT32 file\n";

    for (uint32_t i = 0; i + 1U < sizeof(edit_text); i++) {
        dest[i] = (uint8_t)edit_text[i];
    }
}

int main(int argc, char **argv) {
    FILE *shell = NULL;
    FILE *hello = NULL;
    FILE *output = NULL;
    uint8_t *image = NULL;
    long shell_size_long;
    long hello_size_long = 0;
    uint32_t shell_size;
    uint32_t hello_size = 0U;
    uint32_t shell_cluster_count;
    uint32_t hello_cluster_count = 0U;
    uint32_t edit_first_cluster;
    uint32_t hello_first_cluster;
    uint32_t data_cluster_capacity;
    uint32_t used_cluster_count;
    int result = 1;

    if (argc != 3 && argc != 4) {
        fprintf(stderr,
                "usage: %s output.img shell.bin [external.kli]\n",
                argv[0]);
        return 1;
    }

    shell = fopen(argv[2], "rb");
    if (shell == NULL) {
        perror(argv[2]);
        goto cleanup;
    }
    shell_size_long = file_size(shell);
    if (shell_size_long <= 0 || (uint64_t)shell_size_long > UINT32_MAX) {
        fprintf(stderr, "shell payload size is invalid\n");
        goto cleanup;
    }
    shell_size = (uint32_t)shell_size_long;

    if (argc == 4) {
        hello = fopen(argv[3], "rb");
        if (hello == NULL) {
            perror(argv[3]);
            goto cleanup;
        }
        hello_size_long = file_size(hello);
        if (hello_size_long <= 0 ||
            (uint64_t)hello_size_long > UINT32_MAX) {
            fprintf(stderr, "external payload size is invalid\n");
            goto cleanup;
        }
        hello_size = (uint32_t)hello_size_long;
    }

    shell_cluster_count = clusters_for_size(shell_size);
    hello_cluster_count = hello_size == 0U ? 0U : clusters_for_size(hello_size);
    edit_first_cluster = SHELL_FIRST_CLUSTER + shell_cluster_count;
    hello_first_cluster = edit_first_cluster + EDIT_CLUSTER_COUNT;
    data_cluster_capacity = TOTAL_SECTORS - data_start_sector();
    used_cluster_count = 1U + shell_cluster_count + EDIT_CLUSTER_COUNT +
                         hello_cluster_count;
    if (used_cluster_count > data_cluster_capacity) {
        fprintf(stderr, "payloads do not fit in tiny FAT32 image\n");
        goto cleanup;
    }

    image = (uint8_t *)calloc(1, IMAGE_SIZE);
    if (image == NULL) {
        goto cleanup;
    }

    write_boot_sector(image);
    write_fat(image, shell_cluster_count, edit_first_cluster,
              hello_cluster_count, hello_first_cluster);
    write_root_entries(image, shell_size, edit_first_cluster,
                       hello_size, hello_first_cluster);
    if (copy_payload(image, shell, shell_size, SHELL_FIRST_CLUSTER) != 0) {
        fprintf(stderr, "failed to copy shell payload\n");
        goto cleanup;
    }
    write_edit_payload(image, edit_first_cluster);
    if (hello != NULL &&
        copy_payload(image, hello, hello_size, hello_first_cluster) != 0) {
        fprintf(stderr, "failed to copy external payload\n");
        goto cleanup;
    }

    output = fopen(argv[1], "wb");
    if (output == NULL) {
        perror(argv[1]);
        goto cleanup;
    }
    if (fwrite(image, 1, IMAGE_SIZE, output) != IMAGE_SIZE) {
        fprintf(stderr, "failed to write image\n");
        goto cleanup;
    }

    result = 0;

cleanup:
    if (output != NULL) {
        fclose(output);
    }
    if (hello != NULL) {
        fclose(hello);
    }
    if (shell != NULL) {
        fclose(shell);
    }
    free(image);
    return result;
}

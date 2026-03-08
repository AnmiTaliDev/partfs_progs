/*
 * PartFS - partfs-progs
 * mkfs/main.c - mkfs.part entry point
 *
 * Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include <crc32c.h>
#include "mkfs.h"
#include "geometry.h"
#include "io.h"
#include "superblock.h"
#include "journal.h"
#include "group.h"
#include "inode.h"

static void generate_uuid(uint8_t uuid[16])
{
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
        die("open /dev/urandom");
    if (read(fd, uuid, 16) != 16)
        die("read /dev/urandom");
    close(fd);

    /* RFC 4122 version 4, variant 1 */
    uuid[6] = (uint8_t)((uuid[6] & 0x0Fu) | 0x40u);
    uuid[8] = (uint8_t)((uuid[8] & 0x3Fu) | 0x80u);
}

static uint64_t now_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        die("clock_gettime");
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

static _Noreturn void version(void)
{
    printf("mkfs.part (partfs-progs) %d.%d\n",
           PARTFS_VERSION_MAJOR, PARTFS_VERSION_MINOR);
    exit(EXIT_SUCCESS);
}

static _Noreturn void usage(void)
{
    fprintf(stderr,
        "Usage: mkfs.part [-L label] [-j journal_blocks] <device|file>\n"
        "\n"
        "  -L label          Volume label (up to 32 bytes UTF-8)\n"
        "  -j journal_blocks Journal size in blocks (default %d, block = 4096 B)\n"
        "  --version         Print version and exit\n",
        PARTFS_JOURNAL_DEFAULT);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    const char *label        = NULL;
    uint32_t    journal_blks = PARTFS_JOURNAL_DEFAULT;
    const char *device       = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-L") == 0) {
            if (++i >= argc) usage();
            label = argv[i];
        } else if (strcmp(argv[i], "-j") == 0) {
            if (++i >= argc) usage();
            char *end;
            unsigned long v = strtoul(argv[i], &end, 10);
            if (*end || v == 0 || v > 0xFFFFFFFFu)
                die_msg("invalid journal block count");
            journal_blks = (uint32_t)v;
        } else if (strcmp(argv[i], "--version") == 0) {
            version();
        } else if (argv[i][0] == '-') {
            usage();
        } else {
            device = argv[i];
        }
    }

    if (!device)
        usage();

    crc32c_init();

    int fd = open(device, O_RDWR);
    if (fd < 0)
        die(device);

    uint64_t device_bytes = io_detect_size(fd, device);

    if (device_bytes / PARTFS_BLOCK_SIZE < PARTFS_MIN_BLOCKS(journal_blks)) {
        fprintf(stderr,
            "mkfs.part: device too small: need at least %llu blocks (%llu MiB)\n",
            (unsigned long long)PARTFS_MIN_BLOCKS(journal_blks),
            (unsigned long long)PARTFS_MIN_BLOCKS(journal_blks)
                * PARTFS_BLOCK_SIZE / (1024 * 1024));
        exit(EXIT_FAILURE);
    }

    mkfs_geo_t g = geo_compute(device_bytes, journal_blks);

    printf("Creating PartFS on %s ...", device);
    fflush(stdout);

    uint8_t  uuid[16];
    uint64_t ts = now_ns();
    generate_uuid(uuid);

    /* --- Pass 1: write initial superblocks (free_blocks/root_inode TBD) --- */
    struct partfs_superblock sb;
    sb_fill(&sb, &g, uuid, label, ts);
    sb_update_crc(&sb);
    sb_write(fd, PARTFS_LBA_SUPERBLOCK, &sb);
    sb_write(fd, PARTFS_LBA_BACKUP_SB,  &sb);

    /* --- Journal: two identical header blocks + zeroed data area --- */
    journal_write_header(fd, PARTFS_LBA_JOURNAL_HDR,     &g);
    journal_write_header(fd, PARTFS_LBA_JOURNAL_HDR + 1, &g);
    io_write_zeros(fd, PARTFS_LBA_JOURNAL_DAT, journal_blks);

    /* --- Block groups --- */
    for (uint64_t gi = 0; gi < g.num_groups; gi++) {
        uint64_t base       = g.groups_start + gi * PARTFS_GROUP_SIZE;
        uint64_t n_blocks   = geo_blocks_in_group(&g, gi);
        uint64_t bitmap_lba = base + GRP_OFF_BITMAP;
        uint64_t inode_tree_lba;
        uint64_t data_lba;
        uint32_t used_count;

        if (gi == 0) {
            inode_tree_lba = base + GRP_OFF_INODE_TREE;
            data_lba       = base + GRP_OFF_DIR_DATA;
            used_count     = PARTFS_GRP_META_G0;
        } else {
            inode_tree_lba = 0;
            data_lba       = base + PARTFS_GRP_META_GN;
            used_count     = PARTFS_GRP_META_GN;
        }

        uint64_t free_blks = (n_blocks > used_count)
                             ? (n_blocks - used_count) : 0;

        group_write_descriptor(fd, base + GRP_OFF_DESCRIPTOR,
                               gi, bitmap_lba, inode_tree_lba,
                               data_lba, free_blks, n_blocks);
        group_write_bitmap(fd, bitmap_lba, used_count);
    }

    /* --- Root inode and directory data --- */
    uint64_t g0_base      = g.groups_start;
    uint64_t inode_blk    = g0_base + GRP_OFF_INODE_TREE;
    uint64_t dir_data_lba = g0_base + GRP_OFF_DIR_DATA;

    inode_write_btree(fd, inode_blk, dir_data_lba, ts);
    inode_write_dir_data(fd, dir_data_lba);

    /* --- Pass 2: patch and rewrite superblocks with final counts --- */
    sb.free_blocks = htole64(geo_free_blocks(&g));
    sb.root_inode  = htole64(ROOT_INODE_NO);
    sb.inode_count = htole64(1);
    sb_update_crc(&sb);
    sb_write(fd, PARTFS_LBA_SUPERBLOCK, &sb);
    sb_write(fd, PARTFS_LBA_BACKUP_SB,  &sb);

    if (fsync(fd) != 0)
        die("fsync");

    close(fd);
    printf(" done.\n");
    return 0;
}

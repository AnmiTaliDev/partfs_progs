/*
 * PartFS - partfs-progs
 * mkfs/group.c - block group initialisation
 *
 * Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <string.h>
#include <endian.h>

#include "mkfs.h"
#include "io.h"
#include "group.h"

void group_write_descriptor(int fd,
                             uint64_t lba,
                             uint64_t gidx,
                             uint64_t bitmap_lba,
                             uint64_t inode_tree_lba,
                             uint64_t data_lba,
                             uint64_t free_blks,
                             uint64_t total_blks)
{
    uint8_t block[PARTFS_BLOCK_SIZE];
    memset(block, 0, sizeof(block));

    struct partfs_group_desc *gd = (struct partfs_group_desc *)block;
    gd->hdr.magic       = htole32(PARTFS_MAGIC_GRPD);
    gd->hdr.block_no    = htole64(lba);
    gd->group_no        = htole64(gidx);
    gd->bitmap_start    = htole64(bitmap_lba);
    gd->inode_tree_root = htole64(inode_tree_lba);
    gd->data_start      = htole64(data_lba);
    gd->free_blocks     = htole64(free_blks);
    gd->total_blocks    = htole64(total_blks);
    gd->flags           = htole32(0);
    gd->hdr.crc32c      = htole32(io_block_crc(block));

    io_write_block(fd, lba, block);
}

void group_write_bitmap(int fd, uint64_t bitmap_lba, uint32_t used_count)
{
    uint8_t bitmap[PARTFS_BLOCK_SIZE * PARTFS_BITMAP_BLOCKS];
    memset(bitmap, 0, sizeof(bitmap));

    for (uint32_t i = 0; i < used_count; i++)
        bitmap[i / 8] |= (uint8_t)(1u << (i % 8));

    for (uint32_t b = 0; b < PARTFS_BITMAP_BLOCKS; b++)
        io_write_block(fd, bitmap_lba + b, bitmap + (size_t)b * PARTFS_BLOCK_SIZE);
}

/*
 * PartFS - partfs-progs
 * mkfs/superblock.c - superblock initialisation and write
 *
 * Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <string.h>
#include <endian.h>

#include <crc32c.h>
#include "mkfs.h"
#include "io.h"
#include "superblock.h"

void sb_fill(struct partfs_superblock *sb,
             const mkfs_geo_t *g,
             const uint8_t uuid[16],
             const char *label,
             uint64_t ts)
{
    memset(sb, 0, sizeof(*sb));

    sb->magic          = htole32(PARTFS_MAGIC_SB);
    sb->version_major  = htole16(PARTFS_VERSION_MAJOR);
    sb->version_minor  = htole16(PARTFS_VERSION_MINOR);
    sb->block_count    = htole64(g->total_blocks);
    sb->free_blocks    = htole64(0);              /* patched after group pass */
    sb->journal_start  = htole64(PARTFS_LBA_JOURNAL_HDR);
    sb->journal_blocks = htole32(g->journal_blocks);
    sb->group_size     = htole32(PARTFS_GROUP_SIZE);
    sb->root_inode     = htole64(0);              /* patched after inode write */
    sb->inode_count    = htole64(0);              /* patched after inode write */

    memcpy(sb->uuid, uuid, 16);

    if (label && label[0]) {
        size_t n = strlen(label);
        if (n > sizeof(sb->label))
            n = sizeof(sb->label);
        memcpy(sb->label, label, n);
    }

    sb->mkfs_time  = htole64(ts);
    sb->mount_count = htole64(0);
    sb->flags      = htole32(PARTFS_FLAG_CLEAN  | PARTFS_FLAG_JOURNAL |
                              PARTFS_FLAG_CHECKSUMS | PARTFS_FLAG_EXTENTS);
    sb->crc32c     = htole32(0);
}

void sb_update_crc(struct partfs_superblock *sb)
{
    sb->crc32c = htole32(0);
    sb->crc32c = htole32(crc32c_compute(0, sb, PARTFS_SB_CRC_LENGTH));
}

void sb_write(int fd, uint64_t lba, const struct partfs_superblock *sb)
{
    uint8_t block[PARTFS_BLOCK_SIZE];
    memset(block, 0, sizeof(block));
    memcpy(block, sb, sizeof(*sb));
    io_write_block(fd, lba, block);
}

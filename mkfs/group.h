/*
 * PartFS - partfs-progs
 * mkfs/group.h - block group initialisation
 *
 * Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include <stdint.h>

/* Write the group descriptor block for group @gidx at @lba. */
void group_write_descriptor(int fd,
                             uint64_t lba,
                             uint64_t gidx,
                             uint64_t bitmap_lba,
                             uint64_t inode_tree_lba,
                             uint64_t data_lba,
                             uint64_t free_blks,
                             uint64_t total_blks);

/* Write the allocation bitmap for a group, marking the first @used_count
 * blocks as allocated. Writes PARTFS_BITMAP_BLOCKS consecutive blocks
 * starting at @bitmap_lba. */
void group_write_bitmap(int fd, uint64_t bitmap_lba, uint32_t used_count);

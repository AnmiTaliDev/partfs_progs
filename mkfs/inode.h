/*
 * PartFS - partfs-progs
 * mkfs/inode.h - inode B-tree and root directory
 *
 * Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include <stdint.h>

/* Write the group-0 inode B-tree leaf containing the root directory inode.
 * @dir_data_lba is the LBA of the root directory's data block. */
void inode_write_btree(int fd, uint64_t lba, uint64_t dir_data_lba,
                        uint64_t ts);

/* Write the root directory data block (B-tree leaf with '.' and '..'). */
void inode_write_dir_data(int fd, uint64_t lba);

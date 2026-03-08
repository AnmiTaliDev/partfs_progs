/*
 * PartFS - partfs-progs
 * mkfs/superblock.h - superblock operations
 *
 * Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include <stdint.h>
#include <partfs.h>
#include "geometry.h"

/* Populate @sb with filesystem metadata. free_blocks/root_inode/inode_count
 * are left at zero and must be patched before the final write. */
void sb_fill(struct partfs_superblock *sb,
             const mkfs_geo_t *g,
             const uint8_t uuid[16],
             const char *label,
             uint64_t ts);

/* Recompute and store the CRC32C field of @sb. */
void sb_update_crc(struct partfs_superblock *sb);

/* Write @sb to the device at the given LBA (zero-padded to one block). */
void sb_write(int fd, uint64_t lba, const struct partfs_superblock *sb);

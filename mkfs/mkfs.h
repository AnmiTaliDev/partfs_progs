/*
 * PartFS - partfs-progs
 * mkfs/mkfs.h - shared mkfs definitions
 *
 * Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <partfs.h>

/* Filesystem version */
#define PARTFS_VERSION_MAJOR    1
#define PARTFS_VERSION_MINOR    0

/* Fixed LBA positions */
#define PARTFS_LBA_SUPERBLOCK   UINT64_C(0)
#define PARTFS_LBA_BACKUP_SB    UINT64_C(1)
#define PARTFS_LBA_JOURNAL_HDR  UINT64_C(2)
#define PARTFS_LBA_JOURNAL_DAT  UINT64_C(4)

/* CRC coverage lengths */
#define PARTFS_SB_CRC_LENGTH    124u  /* bytes 0x00-0x7B of superblock */
#define PARTFS_BLK_CRC_OFFSET   4u   /* offsetof(crc32c) in partfs_block_hdr */

/* Per-group block offsets (relative to group base LBA) */
#define GRP_OFF_DESCRIPTOR      0u
#define GRP_OFF_BITMAP          1u   /* 4 blocks */
#define GRP_OFF_INODE_TREE      5u   /* group 0 only */
#define GRP_OFF_DIR_DATA        6u   /* group 0 only */

/* Minimum blocks: 2 superblocks + 2 journal headers + journal data + group 0 */
#define PARTFS_MIN_BLOCKS(j)    (2u + 2u + (uint64_t)(j) + PARTFS_GRP_META_G0)

#define ROOT_INODE_NO   UINT64_C(1)
#define ROOT_DIR_MODE   0755u

/* Volume geometry */
typedef struct {
    uint64_t total_blocks;
    uint64_t groups_start;
    uint64_t num_groups;
    uint32_t journal_blocks;
} mkfs_geo_t;

/* Error helpers - terminate on failure */
_Noreturn void die(const char *msg);
_Noreturn void die_msg(const char *msg);

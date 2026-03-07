/*
 * PartFS - partfs-progs
 * lib/partfs.h
 *
 * Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/* Constants */

#define PARTFS_MAGIC          UINT32_C(0x53465450)  /* 'PTFS' in LE hexdump */
#define PARTFS_VERSION_MAJOR  UINT16_C(1)
#define PARTFS_VERSION_MINOR  UINT16_C(0)
#define PARTFS_BLOCK_SIZE     4096u
#define PARTFS_GROUP_SIZE     32768u   /* blocks per group (~128 MB) */
#define PARTFS_JOURNAL_DEF    1024u    /* default journal size in blocks */
#define PARTFS_BITMAP_BLOCKS  4u       /* alloc bitmap blocks per group */

/* Fixed-region LBAs */
#define PARTFS_LBA_SUPERBLOCK  UINT64_C(0)
#define PARTFS_LBA_BACKUP_SB   UINT64_C(1)
#define PARTFS_LBA_JOURNAL_HDR UINT64_C(2)  /* 2 blocks: 2 and 3 */
#define PARTFS_LBA_JOURNAL_DAT UINT64_C(4)  /* first journal data block */

/* Superblock flags */
#define SB_FLAG_CLEAN      (UINT32_C(1) << 0)
#define SB_FLAG_JOURNAL    (UINT32_C(1) << 1)
#define SB_FLAG_CHECKSUMS  (UINT32_C(1) << 2)
#define SB_FLAG_EXTENTS    (UINT32_C(1) << 3)
#define SB_FLAG_XATTR      (UINT32_C(1) << 4)
#define SB_FLAG_ACL        (UINT32_C(1) << 5)

/* Block-type magic numbers */
#define BTRE_MAGIC  UINT32_C(0x42545245)  /* B-tree internal node */
#define BTLF_MAGIC  UINT32_C(0x42544C46)  /* B-tree leaf node     */
#define JRNL_MAGIC  UINT32_C(0x4A524E4C)  /* journal block        */
#define GRPD_MAGIC  UINT32_C(0x47525044)  /* group descriptor     */
#define COMT_MAGIC  UINT32_C(0x434F4D54)  /* journal commit       */

/* Inode type codes */
#define ITYPE_FILE    UINT16_C(0x0001)
#define ITYPE_DIR     UINT16_C(0x0002)
#define ITYPE_SYMLINK UINT16_C(0x0003)
#define ITYPE_DELETED UINT16_C(0x0004)

/* Inode flags */
#define IFLAG_INLINE    (UINT16_C(1) << 0)
#define IFLAG_SPARSE    (UINT16_C(1) << 1)
#define IFLAG_IMMUTABLE (UINT16_C(1) << 2)
#define IFLAG_APPEND    (UINT16_C(1) << 3)

/* Journal state */
#define JOURNAL_STATE_CLEAN     UINT32_C(0)
#define JOURNAL_STATE_DIRTY     UINT32_C(1)
#define JOURNAL_STATE_REPLAYING UINT32_C(2)

/* Superblock  (block 0, offset 0, exactly 128 bytes) */
struct partfs_superblock {
    uint32_t magic;          /* 0x00: 0x53465450 */
    uint16_t version_major;  /* 0x04 */
    uint16_t version_minor;  /* 0x06 */
    uint64_t block_count;    /* 0x08 */
    uint64_t free_blocks;    /* 0x10 */
    uint64_t journal_start;  /* 0x18: LBA of first journal block */
    uint32_t journal_blocks; /* 0x20 */
    uint32_t group_size;     /* 0x24 */
    uint64_t root_inode;     /* 0x28 */
    uint64_t inode_count;    /* 0x30 */
    uint8_t  uuid[16];       /* 0x38 */
    uint8_t  label[32];      /* 0x48 */
    uint64_t mkfs_time;      /* 0x68 */
    uint64_t mount_count;    /* 0x70 */
    uint32_t flags;          /* 0x78 */
    uint32_t crc32c;         /* 0x7C */
} __attribute__((packed));

_Static_assert(sizeof(struct partfs_superblock) == 128,
               "partfs_superblock must be exactly 128 bytes");

/* CRC covers bytes 0x00..0x7B; field sits at 0x7C */
#define SB_CRC_OFFSET  0x7Cu
#define SB_CRC_LENGTH  0x7Cu

/* Block header  (first 16 bytes of every metadata block) */
struct partfs_block_hdr {
    uint32_t magic;    /* 0x00 */
    uint32_t crc32c;   /* 0x04  (= 0 during checksum calculation) */
    uint64_t block_no; /* 0x08 */
} __attribute__((packed));

_Static_assert(sizeof(struct partfs_block_hdr) == 16,
               "partfs_block_hdr must be exactly 16 bytes");

#define BLK_CRC_OFFSET  4u

/* Journal header block  (blocks 2 and 3, one copy each) */
struct partfs_journal_hdr {
    struct partfs_block_hdr hdr;   /* 0x00: magic = JRNL_MAGIC */
    uint64_t seq_head;             /* 0x10 */
    uint64_t seq_tail;             /* 0x18 */
    uint64_t journal_start;        /* 0x20: LBA of first journal data block */
    uint32_t journal_size;         /* 0x28 */
    uint32_t state;                /* 0x2C: 0 = clean */
    uint8_t  _pad[PARTFS_BLOCK_SIZE - 48];
} __attribute__((packed));

_Static_assert(sizeof(struct partfs_journal_hdr) == PARTFS_BLOCK_SIZE,
               "partfs_journal_hdr must fill exactly one block");

/* Block group descriptor  (first block of every group) */
struct partfs_group_desc {
    struct partfs_block_hdr hdr;   /* 0x00: magic = GRPD_MAGIC */
    uint64_t group_no;             /* 0x10 */
    uint64_t bitmap_start;         /* 0x18: LBA of first bitmap block */
    uint64_t inode_tree_root;      /* 0x20: LBA of inode B-tree root (0 = none) */
    uint64_t data_start;           /* 0x28: LBA of first data block */
    uint64_t free_blocks;          /* 0x30 */
    uint64_t total_blocks;         /* 0x38 */
    uint32_t flags;                /* 0x40 */
    uint8_t  _pad[PARTFS_BLOCK_SIZE - 68];
} __attribute__((packed));

_Static_assert(sizeof(struct partfs_group_desc) == PARTFS_BLOCK_SIZE,
               "partfs_group_desc must fill exactly one block");

/* Base inode  (128 bytes) */
struct partfs_inode {
    uint16_t inode_type;    /* 0x00 */
    uint16_t mode;          /* 0x02: rwxrwxrwx + special bits */
    uint32_t uid;           /* 0x04 */
    uint32_t gid;           /* 0x08 */
    uint32_t refcount;      /* 0x0C */
    uint64_t size;          /* 0x10 */
    uint64_t blocks_used;   /* 0x18 */
    uint64_t crtime_ns;     /* 0x20 */
    uint64_t mtime_ns;      /* 0x28 */
    uint64_t inode_no;      /* 0x30 */
    uint16_t extent_count;  /* 0x38 */
    uint16_t xattr_len;     /* 0x3A */
    uint16_t inline_len;    /* 0x3C */
    uint16_t flags;         /* 0x3E */
    uint8_t  tail[64];      /* 0x40: extents / xattr / inline data */
} __attribute__((packed));

_Static_assert(sizeof(struct partfs_inode) == 128,
               "partfs_inode must be exactly 128 bytes");

/* Extent descriptor  (16 bytes) */
struct partfs_extent {
    uint64_t logical_block; /* 0x00 */
    uint8_t  phys_block[6]; /* 0x08: 48-bit LBA, little-endian */
    uint16_t length;        /* 0x0E: length in blocks */
} __attribute__((packed));

_Static_assert(sizeof(struct partfs_extent) == 16,
               "partfs_extent must be exactly 16 bytes");

/* Directory record  (variable length; header = 32 bytes)
 * Filename bytes follow immediately after the header at offset 0x20. */
struct partfs_dirent {
    uint64_t inode_no;   /* 0x00 */
    uint64_t name_hash;  /* 0x08: FNV-1a 64-bit */
    uint16_t name_len;   /* 0x10 */
    uint16_t rec_len;    /* 0x12: total length including name, 4-byte aligned */
    uint16_t inode_type; /* 0x14 */
    uint16_t _reserved;  /* 0x16 */
    uint32_t crc32c;     /* 0x18 */
    uint32_t _padding;   /* 0x1C */
    /* uint8_t name[name_len] follows here at 0x20 */
} __attribute__((packed));

_Static_assert(sizeof(struct partfs_dirent) == 0x20,
               "partfs_dirent header must be exactly 32 bytes");

/* B-tree leaf block header.
 * Used for both the inode B-tree and the directory B-tree.
 * Records follow immediately after at offset 0x18. */
struct partfs_btree_leaf_hdr {
    struct partfs_block_hdr hdr;  /* 0x00 */
    uint32_t entry_count;         /* 0x10 */
    uint32_t _reserved;           /* 0x14 */
} __attribute__((packed));

_Static_assert(sizeof(struct partfs_btree_leaf_hdr) == 24,
               "partfs_btree_leaf_hdr must be 24 bytes");

/*
 * Inode B-tree leaf entry layout (immediately after btree_leaf_hdr):
 *   uint64_t  key        -- inode number
 *   uint32_t  val_size   -- byte size of inode data (base + tail extents)
 *   uint32_t  _reserved
 *   uint8_t   data[val_size]
 */
#define INODE_ENTRY_HDR_SIZE  16u

/* Journal commit record  (32 bytes) */
struct partfs_commit {
    uint64_t seq;          /* 0x00 */
    uint64_t block_count;  /* 0x08 */
    uint64_t timestamp_ns; /* 0x10 */
    uint32_t crc32c;       /* 0x18 */
    uint32_t magic;        /* 0x1C: COMT_MAGIC */
} __attribute__((packed));

_Static_assert(sizeof(struct partfs_commit) == 32,
               "partfs_commit must be exactly 32 bytes");

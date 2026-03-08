/*
 * PartFS - partfs-progs
 * mkfs/inode.c - inode B-tree leaf and root directory data
 *
 * Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <string.h>
#include <endian.h>

#include <crc32c.h>
#include "mkfs.h"
#include "io.h"
#include "inode.h"

/* Encode a 48-bit physical LBA into the 6-byte little-endian field. */
static void set_phys_block(uint8_t field[6], uint64_t lba)
{
    field[0] = (uint8_t)(lba >>  0);
    field[1] = (uint8_t)(lba >>  8);
    field[2] = (uint8_t)(lba >> 16);
    field[3] = (uint8_t)(lba >> 24);
    field[4] = (uint8_t)(lba >> 32);
    field[5] = (uint8_t)(lba >> 40);
}

/* Return the padded record length for a directory entry with @name_len bytes.
 * The fixed header is PARTFS_DIRENT_HDR_SIZE bytes; total is 4-byte aligned. */
static uint16_t dirent_reclen(uint16_t name_len)
{
    return (uint16_t)(((uint32_t)PARTFS_DIRENT_HDR_SIZE + name_len + 3u) & ~3u);
}

/* Compute CRC32C for a single directory record.
 * The crc32c field inside the record is zeroed before hashing. */
static uint32_t dirent_crc(const uint8_t *rec, uint16_t rec_len)
{
    uint8_t tmp[PARTFS_BLOCK_SIZE];
    if (rec_len > sizeof(tmp))
        return 0;
    memcpy(tmp, rec, rec_len);
    memset(tmp + offsetof(struct partfs_dirent, crc32c), 0, sizeof(uint32_t));
    return crc32c_compute(0, tmp, rec_len);
}

/* Serialise one directory entry into @buf at @offset; return bytes written. */
static size_t write_dirent(uint8_t *buf, size_t offset,
                            uint64_t inode_no, uint16_t itype,
                            const char *name)
{
    uint16_t name_len = (uint16_t)strlen(name);
    uint16_t rec_len  = dirent_reclen(name_len);
    uint8_t *p = buf + offset;

    memset(p, 0, rec_len);

    struct partfs_dirent *de = (struct partfs_dirent *)p;
    de->inode_no   = htole64(inode_no);
    de->name_hash  = htole64(fnv1a_64((const uint8_t *)name, name_len));
    de->name_len   = htole16(name_len);
    de->rec_len    = htole16(rec_len);
    de->inode_type = htole16(itype);
    de->reserved   = 0;
    de->padding    = 0;
    memcpy(p + PARTFS_DIRENT_HDR_SIZE, name, name_len);
    de->crc32c     = htole32(dirent_crc(p, rec_len));

    return rec_len;
}

void inode_write_btree(int fd, uint64_t lba, uint64_t dir_data_lba,
                        uint64_t ts)
{
    uint8_t block[PARTFS_BLOCK_SIZE];
    memset(block, 0, sizeof(block));

    struct partfs_btree_leaf_hdr *bh = (struct partfs_btree_leaf_hdr *)block;
    bh->hdr.magic    = htole32(PARTFS_MAGIC_BTLF);
    bh->hdr.block_no = htole64(lba);
    bh->entry_count  = htole32(1);
    bh->reserved     = 0;

    /* Inode entry layout: key(8) + val_size(4) + reserved(4) + inode(128) */
    size_t pos = sizeof(*bh);

    uint64_t key      = htole64(ROOT_INODE_NO);
    uint32_t val_size = htole32(sizeof(struct partfs_inode));
    uint32_t rsvd     = 0;
    memcpy(block + pos, &key,      8); pos += 8;
    memcpy(block + pos, &val_size, 4); pos += 4;
    memcpy(block + pos, &rsvd,     4); pos += 4;

    struct partfs_inode *ino = (struct partfs_inode *)(block + pos);
    ino->inode_type   = htole16(PARTFS_ITYPE_DIR);
    ino->mode         = htole16((uint16_t)ROOT_DIR_MODE);
    ino->uid          = htole32(0);
    ino->gid          = htole32(0);
    ino->refcount     = htole32(2);  /* '.' and '..' */
    ino->size         = htole64(PARTFS_BLOCK_SIZE);
    ino->blocks_used  = htole64(1);
    ino->crtime_ns    = htole64(ts);
    ino->mtime_ns     = htole64(ts);
    ino->inode_no     = htole64(ROOT_INODE_NO);
    ino->extent_count = htole16(1);
    ino->xattr_len    = htole16(0);
    ino->inline_len   = htole16(0);
    ino->flags        = htole16(0);

    /* Single inline extent: logical block 0 → dir_data_lba */
    struct partfs_extent *ext = (struct partfs_extent *)ino->tail;
    ext->logical_block = htole64(0);
    set_phys_block(ext->phys_block, dir_data_lba);
    ext->length = htole16(1);

    bh->hdr.crc32c = htole32(io_block_crc(block));
    io_write_block(fd, lba, block);
}

void inode_write_dir_data(int fd, uint64_t lba)
{
    uint8_t block[PARTFS_BLOCK_SIZE];
    memset(block, 0, sizeof(block));

    struct partfs_btree_leaf_hdr *bh = (struct partfs_btree_leaf_hdr *)block;
    bh->hdr.magic    = htole32(PARTFS_MAGIC_BTLF);
    bh->hdr.block_no = htole64(lba);
    bh->entry_count  = htole32(2);
    bh->reserved     = 0;

    size_t pos = sizeof(*bh);
    pos += write_dirent(block, pos, ROOT_INODE_NO, PARTFS_ITYPE_DIR, ".");
    pos += write_dirent(block, pos, ROOT_INODE_NO, PARTFS_ITYPE_DIR, "..");

    bh->hdr.crc32c = htole32(io_block_crc(block));
    io_write_block(fd, lba, block);
}

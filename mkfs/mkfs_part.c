/*
 * PartFS - partfs-progs
 * mkfs/mkfs_part.c
 *
 * Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
 * SPDX-License-Identifier: MIT
 */

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/falloc.h>
#include <linux/fs.h>
#include <time.h>
#include <endian.h>

#include "partfs.h"
#include "crc32c.h"

/* Per-group block offsets (relative to group base LBA) */
#define GRP_OFF_DESCRIPTOR   0u
#define GRP_OFF_BITMAP       1u   /* 4 blocks: 1..4 */
#define GRP_OFF_INODE_TREE   5u   /* inode B-tree root leaf (group 0 only) */
#define GRP_OFF_DIR_DATA     6u   /* root dir data (group 0 only) */
#define GRP_META_USED_G0     7u   /* blocks used in group 0 */
/* Groups 1+ have no inode B-tree at mkfs time; the FUSE driver must look up
 * inodes through group 0's tree until per-group trees are created on demand. */
#define GRP_META_USED_GN     5u   /* blocks used in groups 1+: descriptor + 4 bitmap */

/* Minimum blocks required: 2 superblocks + 2 journal headers + journal data + group 0 */
#define PARTFS_MIN_BLOCKS(jblks)  (2u + 2u + (jblks) + GRP_META_USED_G0)

#define ROOT_INODE_NO   UINT64_C(1)
#define ROOT_DIR_MODE   0755u

static void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

static void die_msg(const char *msg)
{
    fprintf(stderr, "mkfs.part: %s\n", msg);
    exit(EXIT_FAILURE);
}

typedef struct {
    uint64_t total_blocks;
    uint64_t groups_start;
    uint64_t num_groups;
    uint32_t journal_blocks;
} geometry_t;

static geometry_t compute_geometry(uint64_t device_bytes, uint32_t journal_blocks)
{
    geometry_t g;
    g.total_blocks   = device_bytes / PARTFS_BLOCK_SIZE;
    g.journal_blocks = journal_blocks;
    g.groups_start   = PARTFS_LBA_JOURNAL_DAT + journal_blocks;

    if (g.groups_start >= g.total_blocks)
        die_msg("device too small to hold journal");

    uint64_t group_space = g.total_blocks - g.groups_start;
    g.num_groups = (group_space + PARTFS_GROUP_SIZE - 1) / PARTFS_GROUP_SIZE;

    if (g.num_groups == 0)
        die_msg("device too small: no room for even one block group");

    return g;
}

static uint64_t blocks_in_group(const geometry_t *g, uint64_t gidx)
{
    uint64_t base  = g->groups_start + gidx * PARTFS_GROUP_SIZE;
    uint64_t avail = g->total_blocks - base;
    return (avail < PARTFS_GROUP_SIZE) ? avail : PARTFS_GROUP_SIZE;
}

static uint64_t compute_free_blocks(const geometry_t *g)
{
    uint64_t total_free = 0;
    for (uint64_t i = 0; i < g->num_groups; i++) {
        uint64_t n    = blocks_in_group(g, i);
        uint64_t used = (i == 0) ? GRP_META_USED_G0 : GRP_META_USED_GN;
        if (n > used)
            total_free += n - used;
    }
    return total_free;
}

static void write_block(int fd, uint64_t lba, const void *data)
{
    off_t off = (off_t)lba * PARTFS_BLOCK_SIZE;
    if (lseek(fd, off, SEEK_SET) == (off_t)-1)
        die("lseek");
    if (write(fd, data, PARTFS_BLOCK_SIZE) != PARTFS_BLOCK_SIZE)
        die("write");
}

static void write_zeros(int fd, uint64_t lba, uint64_t count)
{
    off_t off = (off_t)lba * PARTFS_BLOCK_SIZE;
    off_t len = (off_t)count * PARTFS_BLOCK_SIZE;

    /* FALLOC_FL_ZERO_RANGE is fast on regular files (no actual I/O);
     * falls back to explicit writes for block devices or older kernels. */
    if (fallocate(fd, FALLOC_FL_ZERO_RANGE, off, len) == 0)
        return;

    static const uint8_t zero[PARTFS_BLOCK_SIZE];
    for (uint64_t i = 0; i < count; i++)
        write_block(fd, lba + i, zero);
}

static void generate_uuid(uint8_t uuid[16])
{
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
        die("open /dev/urandom");
    if (read(fd, uuid, 16) != 16)
        die("read /dev/urandom");
    close(fd);

    /* RFC 4122 version 4, variant bits */
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

/* Write 48-bit LBA into a 6-byte little-endian field */
static void set_phys_block(uint8_t field[6], uint64_t lba)
{
    field[0] = (uint8_t)(lba >> 0);
    field[1] = (uint8_t)(lba >> 8);
    field[2] = (uint8_t)(lba >> 16);
    field[3] = (uint8_t)(lba >> 24);
    field[4] = (uint8_t)(lba >> 32);
    field[5] = (uint8_t)(lba >> 40);
}

/* FNV-1a 64-bit hash — directory B-tree key */
static uint64_t fnv1a_64(const uint8_t *data, size_t len)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint64_t)data[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void fill_superblock(struct partfs_superblock *sb,
                             const geometry_t *g,
                             const uint8_t uuid[16],
                             const char *label,
                             uint64_t ts)
{
    memset(sb, 0, sizeof(*sb));

    sb->magic          = htole32(PARTFS_MAGIC);
    sb->version_major  = htole16(PARTFS_VERSION_MAJOR);
    sb->version_minor  = htole16(PARTFS_VERSION_MINOR);
    sb->block_count    = htole64(g->total_blocks);
    sb->free_blocks    = htole64(0);                /* filled in at the end */
    sb->journal_start  = htole64(PARTFS_LBA_JOURNAL_HDR);
    sb->journal_blocks = htole32(g->journal_blocks);
    sb->group_size     = htole32(PARTFS_GROUP_SIZE);
    sb->root_inode     = htole64(0);                /* filled in at the end */
    sb->inode_count    = htole64(0);                /* filled in at the end */
    memcpy(sb->uuid, uuid, 16);
    if (label && label[0]) {
        size_t llen = strlen(label);
        if (llen > sizeof(sb->label))
            llen = sizeof(sb->label);
        memcpy(sb->label, label, llen);
    }
    sb->mkfs_time   = htole64(ts);
    sb->mount_count = htole64(0);
    sb->flags       = htole32(SB_FLAG_CLEAN | SB_FLAG_JOURNAL |
                               SB_FLAG_CHECKSUMS | SB_FLAG_EXTENTS);
    sb->crc32c      = htole32(0);
}

static void superblock_set_crc(struct partfs_superblock *sb)
{
    sb->crc32c = htole32(crc32c_compute(0, sb, SB_CRC_LENGTH));
}

static void write_superblock(int fd, uint64_t lba,
                              const struct partfs_superblock *sb)
{
    uint8_t block[PARTFS_BLOCK_SIZE];
    memset(block, 0, sizeof(block));
    memcpy(block, sb, sizeof(*sb));
    write_block(fd, lba, block);
}

static void write_journal_header(int fd, uint64_t lba, const geometry_t *g)
{
    struct partfs_journal_hdr jh;
    memset(&jh, 0, sizeof(jh));

    jh.hdr.magic     = htole32(JRNL_MAGIC);
    jh.hdr.block_no  = htole64(lba);
    jh.seq_head      = htole64(0);
    jh.seq_tail      = htole64(0);
    jh.journal_start = htole64(PARTFS_LBA_JOURNAL_DAT);
    jh.journal_size  = htole32(g->journal_blocks);
    jh.state         = htole32(JOURNAL_STATE_CLEAN);
    jh.hdr.crc32c    = htole32(crc32c_block(&jh, sizeof(jh), BLK_CRC_OFFSET));

    write_block(fd, lba, &jh);
}

static void write_group_descriptor(int fd, uint64_t lba,
                                    uint64_t gidx,
                                    uint64_t bitmap_lba,
                                    uint64_t inode_tree_lba,
                                    uint64_t data_lba,
                                    uint64_t free_blks,
                                    uint64_t total_blks)
{
    struct partfs_group_desc gd;
    memset(&gd, 0, sizeof(gd));

    gd.hdr.magic       = htole32(GRPD_MAGIC);
    gd.hdr.block_no    = htole64(lba);
    gd.group_no        = htole64(gidx);
    gd.bitmap_start    = htole64(bitmap_lba);
    gd.inode_tree_root = htole64(inode_tree_lba);
    gd.data_start      = htole64(data_lba);
    gd.free_blocks     = htole64(free_blks);
    gd.total_blocks    = htole64(total_blks);
    gd.flags           = htole32(0);
    gd.hdr.crc32c      = htole32(crc32c_block(&gd, sizeof(gd), BLK_CRC_OFFSET));

    write_block(fd, lba, &gd);
}

/* Mark the first @used_count blocks as allocated (bit = 1) */
static void write_bitmap(int fd, uint64_t bitmap_lba, uint32_t used_count)
{
    uint8_t bitmap[PARTFS_BLOCK_SIZE * PARTFS_BITMAP_BLOCKS];
    memset(bitmap, 0, sizeof(bitmap));

    for (uint32_t i = 0; i < used_count; i++) {
        uint32_t byte = i / 8;
        uint32_t bit  = i % 8;
        bitmap[byte] |= (uint8_t)(1u << bit);
    }

    for (uint32_t b = 0; b < PARTFS_BITMAP_BLOCKS; b++)
        write_block(fd, bitmap_lba + b, bitmap + b * PARTFS_BLOCK_SIZE);
}

/* Inode B-tree leaf (group 0): stores root inode #1.
 * Entry layout after btree_leaf_hdr: [u64 key][u32 val_size][u32 pad][inode data] */
static void write_inode_btree(int fd, uint64_t lba, uint64_t dir_data_lba,
                               uint64_t ts)
{
    uint8_t block[PARTFS_BLOCK_SIZE];
    memset(block, 0, sizeof(block));

    struct partfs_btree_leaf_hdr *bh = (struct partfs_btree_leaf_hdr *)block;
    bh->hdr.magic   = htole32(BTLF_MAGIC);
    bh->hdr.block_no= htole64(lba);
    bh->entry_count = htole32(1);
    bh->_reserved   = 0;

    size_t pos = sizeof(*bh);
    uint64_t key      = htole64(ROOT_INODE_NO);
    /* extent lives inside tail[], which is already part of partfs_inode */
    uint32_t val_size = htole32(sizeof(struct partfs_inode));
    uint32_t reserved = 0;
    memcpy(block + pos, &key,      8); pos += 8;
    memcpy(block + pos, &val_size, 4); pos += 4;
    memcpy(block + pos, &reserved, 4); pos += 4;

    struct partfs_inode *ino = (struct partfs_inode *)(block + pos);
    ino->inode_type   = htole16(ITYPE_DIR);
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

    /* Single extent: logical block 0 maps to dir_data_lba */
    struct partfs_extent *ext = (struct partfs_extent *)ino->tail;
    ext->logical_block = htole64(0);
    set_phys_block(ext->phys_block, dir_data_lba);
    ext->length = htole16(1);

    bh->hdr.crc32c = htole32(crc32c_block(block, sizeof(block), BLK_CRC_OFFSET));
    write_block(fd, lba, block);
}

/* Returns rec_len for a given name_len (4-byte aligned, base = 32) */
static uint16_t dirent_reclen(uint16_t name_len)
{
    return (uint16_t)(((uint32_t)sizeof(struct partfs_dirent) + name_len + 3u) & ~3u);
}

static uint32_t dirent_crc(const uint8_t *rec, uint16_t rec_len)
{
    /* A directory record cannot exceed one block */
    uint8_t tmp[PARTFS_BLOCK_SIZE];
    if (rec_len > sizeof(tmp))
        return 0;
    memcpy(tmp, rec, rec_len);
    memset(tmp + offsetof(struct partfs_dirent, crc32c), 0, 4);
    return crc32c_compute(0, tmp, rec_len);
}

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
    de->_reserved  = 0;
    de->_padding   = 0;
    memcpy(p + sizeof(struct partfs_dirent), name, name_len);
    de->crc32c     = htole32(dirent_crc(p, rec_len));

    return rec_len;
}

/* Root directory data block: B-tree leaf with '.' and '..' */
static void write_dir_data(int fd, uint64_t lba)
{
    uint8_t block[PARTFS_BLOCK_SIZE];
    memset(block, 0, sizeof(block));

    struct partfs_btree_leaf_hdr *bh = (struct partfs_btree_leaf_hdr *)block;
    bh->hdr.magic    = htole32(BTLF_MAGIC);
    bh->hdr.block_no = htole64(lba);
    bh->entry_count  = htole32(2);
    bh->_reserved    = 0;

    size_t pos = sizeof(*bh);
    pos += write_dirent(block, pos, ROOT_INODE_NO, ITYPE_DIR, ".");
    pos += write_dirent(block, pos, ROOT_INODE_NO, ITYPE_DIR, "..");

    bh->hdr.crc32c = htole32(crc32c_block(block, sizeof(block), BLK_CRC_OFFSET));
    write_block(fd, lba, block);
}

static uint64_t detect_size(int fd, const char *path)
{
    struct stat st;
    if (fstat(fd, &st) != 0)
        die("fstat");

    if (S_ISREG(st.st_mode))
        return (uint64_t)st.st_size;

    if (S_ISBLK(st.st_mode)) {
        uint64_t bytes = 0;
        if (ioctl(fd, BLKGETSIZE64, &bytes) == 0)
            return bytes;
    }

    fprintf(stderr, "mkfs.part: cannot determine size of '%s'\n", path);
    exit(EXIT_FAILURE);
}

static void usage(void)
{
    fprintf(stderr,
        "Usage: mkfs.part [-L label] [-j journal_blocks] <device|file>\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    const char *label        = NULL;
    uint32_t    journal_blks = PARTFS_JOURNAL_DEF;
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
        } else if (argv[i][0] == '-') {
            usage();
        } else {
            device = argv[i];
        }
    }

    if (!device)
        usage();

    int fd = open(device, O_RDWR);
    if (fd < 0)
        die(device);

    uint64_t device_bytes = detect_size(fd, device);

    if (device_bytes / PARTFS_BLOCK_SIZE < PARTFS_MIN_BLOCKS(journal_blks)) {
        fprintf(stderr,
            "mkfs.part: device too small: need at least %llu blocks (%llu MiB)\n",
            (unsigned long long)PARTFS_MIN_BLOCKS(journal_blks),
            (unsigned long long)PARTFS_MIN_BLOCKS(journal_blks) * PARTFS_BLOCK_SIZE
                / (1024 * 1024));
        exit(EXIT_FAILURE);
    }

    geometry_t g = compute_geometry(device_bytes, journal_blks);

    printf("Creating PartFS on %s ...", device);
    fflush(stdout);

    uint8_t  uuid[16];
    uint64_t ts = now_ns();
    generate_uuid(uuid);

    struct partfs_superblock sb;
    fill_superblock(&sb, &g, uuid, label, ts);
    superblock_set_crc(&sb);
    write_superblock(fd, PARTFS_LBA_SUPERBLOCK, &sb);
    write_superblock(fd, PARTFS_LBA_BACKUP_SB,  &sb);

    write_journal_header(fd, PARTFS_LBA_JOURNAL_HDR,     &g);
    write_journal_header(fd, PARTFS_LBA_JOURNAL_HDR + 1, &g);

    write_zeros(fd, PARTFS_LBA_JOURNAL_DAT, journal_blks);

    for (uint64_t gi = 0; gi < g.num_groups; gi++) {
        uint64_t base       = g.groups_start + gi * PARTFS_GROUP_SIZE;
        uint64_t n_blocks   = blocks_in_group(&g, gi);
        uint64_t bitmap_lba = base + GRP_OFF_BITMAP;

        uint64_t inode_tree_lba;
        uint64_t data_lba;
        uint32_t used_count;

        if (gi == 0) {
            inode_tree_lba = base + GRP_OFF_INODE_TREE;
            data_lba       = base + GRP_OFF_DIR_DATA;
            used_count     = GRP_META_USED_G0;
        } else {
            inode_tree_lba = 0;
            data_lba       = base + GRP_META_USED_GN;
            used_count     = GRP_META_USED_GN;
        }

        uint64_t free_blks = (n_blocks > used_count) ? (n_blocks - used_count) : 0;

        write_group_descriptor(fd, base + GRP_OFF_DESCRIPTOR,
                                gi, bitmap_lba, inode_tree_lba,
                                data_lba, free_blks, n_blocks);
        write_bitmap(fd, bitmap_lba, used_count);
    }

    uint64_t g0_base       = g.groups_start;
    uint64_t inode_blk_lba = g0_base + GRP_OFF_INODE_TREE;
    uint64_t dir_data_lba  = g0_base + GRP_OFF_DIR_DATA;

    write_inode_btree(fd, inode_blk_lba, dir_data_lba, ts);
    write_dir_data(fd, dir_data_lba);

    sb.free_blocks = htole64(compute_free_blocks(&g));
    sb.root_inode  = htole64(ROOT_INODE_NO);
    sb.inode_count = htole64(1);
    sb.crc32c      = htole32(0);
    superblock_set_crc(&sb);

    write_superblock(fd, PARTFS_LBA_SUPERBLOCK, &sb);
    write_superblock(fd, PARTFS_LBA_BACKUP_SB,  &sb);

    if (fsync(fd) != 0)
        die("fsync");

    close(fd);

    printf(" done.\n");
    return 0;
}

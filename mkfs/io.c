/*
 * PartFS - partfs-progs
 * mkfs/io.c - block I/O primitives
 *
 * Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/falloc.h>
#include <linux/fs.h>
#include <fcntl.h>

#include <crc32c.h>
#include "mkfs.h"
#include "io.h"

_Noreturn void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

_Noreturn void die_msg(const char *msg)
{
    fprintf(stderr, "mkfs.part: %s\n", msg);
    exit(EXIT_FAILURE);
}

void io_write_block(int fd, uint64_t lba, const void *block)
{
    off_t off = (off_t)lba * PARTFS_BLOCK_SIZE;
    if (lseek(fd, off, SEEK_SET) == (off_t)-1)
        die("lseek");
    if (write(fd, block, PARTFS_BLOCK_SIZE) != PARTFS_BLOCK_SIZE)
        die("write");
}

void io_write_zeros(int fd, uint64_t lba, uint64_t count)
{
    off_t off = (off_t)lba * PARTFS_BLOCK_SIZE;
    off_t len = (off_t)count * PARTFS_BLOCK_SIZE;

    /* FALLOC_FL_ZERO_RANGE is fast on regular files; fall back for block
     * devices or kernels that do not support it. */
    if (fallocate(fd, FALLOC_FL_ZERO_RANGE, off, len) == 0)
        return;

    static const uint8_t zero[PARTFS_BLOCK_SIZE];
    for (uint64_t i = 0; i < count; i++)
        io_write_block(fd, lba + i, zero);
}

uint64_t io_detect_size(int fd, const char *path)
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

uint32_t io_block_crc(const uint8_t buf[PARTFS_BLOCK_SIZE])
{
    uint8_t tmp[PARTFS_BLOCK_SIZE];
    memcpy(tmp, buf, PARTFS_BLOCK_SIZE);
    /* Zero the crc32c field inside partfs_block_hdr (bytes 4-7). */
    memset(tmp + PARTFS_BLK_CRC_OFFSET, 0, sizeof(uint32_t));
    return crc32c_compute(0, tmp, PARTFS_BLOCK_SIZE);
}

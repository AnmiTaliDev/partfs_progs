/*
 * PartFS - partfs-progs
 * mkfs/io.h - block I/O primitives
 *
 * Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include <stdint.h>
#include <partfs.h>

/* Write exactly one 4096-byte block at the given LBA. */
void io_write_block(int fd, uint64_t lba, const void *block);

/* Zero @count blocks starting at @lba. Uses fallocate when possible. */
void io_write_zeros(int fd, uint64_t lba, uint64_t count);

/* Return device/file size in bytes. Terminates on error. */
uint64_t io_detect_size(int fd, const char *path);

/*
 * Compute the CRC32C for a block_hdr-prefixed on-disk block.
 * @buf must be exactly PARTFS_BLOCK_SIZE bytes with partfs_block_hdr at
 * offset 0.  The crc32c field (bytes 4-7) is zeroed before hashing.
 */
uint32_t io_block_crc(const uint8_t buf[PARTFS_BLOCK_SIZE]);

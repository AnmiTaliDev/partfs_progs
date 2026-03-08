/*
 * PartFS - partfs-progs
 * mkfs/journal.c - write-ahead journal initialisation
 *
 * Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <string.h>
#include <endian.h>

#include "mkfs.h"
#include "io.h"
#include "journal.h"

void journal_write_header(int fd, uint64_t lba, const mkfs_geo_t *g)
{
    uint8_t block[PARTFS_BLOCK_SIZE];
    memset(block, 0, sizeof(block));

    struct partfs_journal_hdr *jh = (struct partfs_journal_hdr *)block;
    jh->hdr.magic    = htole32(PARTFS_MAGIC_JRNL);
    jh->hdr.block_no = htole64(lba);
    jh->seq_head     = htole64(0);
    jh->seq_tail     = htole64(0);
    jh->journal_start = htole64(PARTFS_LBA_JOURNAL_DAT);
    jh->journal_size  = htole32(g->journal_blocks);
    jh->state         = htole32(PARTFS_JOURNAL_CLEAN);
    jh->hdr.crc32c    = htole32(io_block_crc(block));

    io_write_block(fd, lba, block);
}

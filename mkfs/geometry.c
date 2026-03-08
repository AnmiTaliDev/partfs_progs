/*
 * PartFS - partfs-progs
 * mkfs/geometry.c - volume geometry computation
 *
 * Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "geometry.h"

mkfs_geo_t geo_compute(uint64_t device_bytes, uint32_t journal_blocks)
{
    mkfs_geo_t g;
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

uint64_t geo_blocks_in_group(const mkfs_geo_t *g, uint64_t gidx)
{
    uint64_t base  = g->groups_start + gidx * PARTFS_GROUP_SIZE;
    uint64_t avail = g->total_blocks - base;
    return (avail < PARTFS_GROUP_SIZE) ? avail : (uint64_t)PARTFS_GROUP_SIZE;
}

uint64_t geo_free_blocks(const mkfs_geo_t *g)
{
    uint64_t total_free = 0;
    for (uint64_t i = 0; i < g->num_groups; i++) {
        uint64_t n    = geo_blocks_in_group(g, i);
        uint64_t used = (i == 0) ? PARTFS_GRP_META_G0 : PARTFS_GRP_META_GN;
        if (n > used)
            total_free += n - used;
    }
    return total_free;
}

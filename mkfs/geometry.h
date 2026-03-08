/*
 * PartFS - partfs-progs
 * mkfs/geometry.h - volume geometry
 *
 * Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include "mkfs.h"

/* Compute volume geometry from device size and journal block count. */
mkfs_geo_t geo_compute(uint64_t device_bytes, uint32_t journal_blocks);

/* Return the number of blocks in a given group. */
uint64_t geo_blocks_in_group(const mkfs_geo_t *g, uint64_t gidx);

/* Return total free blocks across all groups after mkfs. */
uint64_t geo_free_blocks(const mkfs_geo_t *g);

/*
 * PartFS - partfs-progs
 * mkfs/journal.h - write-ahead journal initialisation
 *
 * Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include <stdint.h>
#include "geometry.h"

/* Write a clean journal header block at @lba.
 * Both primary (LBA 2) and mirror (LBA 3) are written by the caller. */
void journal_write_header(int fd, uint64_t lba, const mkfs_geo_t *g);

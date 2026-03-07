/*
 * PartFS - partfs-progs
 * lib/crc32c.h
 *
 * Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

/*
 * crc32c_compute - compute CRC32C (Castagnoli polynomial).
 *
 * crc  - initial value (typically 0).
 * buf  - pointer to data.
 * len  - byte count.
 *
 * Returns the resulting checksum.
 */
uint32_t crc32c_compute(uint32_t crc, const void *buf, size_t len);

/*
 * crc32c_block - compute CRC32C for a metadata block of @size bytes.
 *
 * Zeroes the 4-byte field at @crc_field_offset before calculating,
 * as required by the PartFS spec ("this field = 0 during calc").
 *
 * block            - pointer to block data (not modified).
 * size             - block size in bytes.
 * crc_field_offset - byte offset of the crc32c field within the block.
 */
uint32_t crc32c_block(const void *block, size_t size, size_t crc_field_offset);

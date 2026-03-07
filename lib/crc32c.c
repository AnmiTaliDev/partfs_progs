/*
 * PartFS - partfs-progs
 * lib/crc32c.c
 *
 * Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "crc32c.h"

/*
 * Castagnoli polynomial in reflected form:
 * x^32+x^28+x^27+x^26+x^25+x^23+x^22+x^20+x^19+x^18+
 * x^14+x^13+x^11+x^10+x^9+x^8+x^6+1  ->  0x82F63B78
 */
#define CRC32C_POLY  UINT32_C(0x82F63B78)

static uint32_t crc_table[256];
static int      table_ready = 0;

static void build_table(void)
{
    for (uint32_t i = 0; i < 256u; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1u)
                crc = (crc >> 1) ^ CRC32C_POLY;
            else
                crc >>= 1;
        }
        crc_table[i] = crc;
    }
    table_ready = 1;
}

uint32_t crc32c_compute(uint32_t crc, const void *buf, size_t len)
{
    if (!table_ready)
        build_table();

    const uint8_t *p = (const uint8_t *)buf;
    crc = ~crc;
    while (len--)
        crc = (crc >> 8) ^ crc_table[(crc ^ *p++) & 0xFFu];
    return ~crc;
}

uint32_t crc32c_block(const void *block, size_t size, size_t crc_field_offset)
{
    uint8_t tmp[4096];
    if (size > sizeof(tmp))
        size = sizeof(tmp);

    memcpy(tmp, block, size);

    /* Zero the crc32c field before computing */
    if (crc_field_offset + 4u <= size)
        memset(tmp + crc_field_offset, 0, 4);

    return crc32c_compute(0, tmp, size);
}

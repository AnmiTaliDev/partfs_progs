# PartFS - partfs-progs
# mkfs/meson.build
#
# Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
# SPDX-License-Identifier: GPL-2.0-only

driver_inc = include_directories('../driver/partfs-fuse/include',
                                  '../driver/partfs-fuse/src')

executable(
  'mkfs.part',
  [
    'main.c',
    'geometry.c',
    'io.c',
    'superblock.c',
    'journal.c',
    'group.c',
    'inode.c',
    '../driver/partfs-fuse/src/crc32c.c',
  ],
  include_directories : driver_inc,
  c_args : ['-D_GNU_SOURCE'],
  install : true,
)

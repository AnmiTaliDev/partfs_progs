# PartFS - partfs-progs
# lib/meson.build
#
# Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
# SPDX-License-Identifier: MIT

partfs_lib = static_library(
  'partfs',
  'crc32c.c',
  include_directories : include_directories('.'),
)

partfs_dep = declare_dependency(
  link_with : partfs_lib,
  include_directories : include_directories('.'),
)

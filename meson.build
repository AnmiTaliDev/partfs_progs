# PartFS - partfs-progs
# meson.build (root)
#
# Copyright (c) 2026 AnmiTaliDev <anmitalidev@nuros.org>
# SPDX-License-Identifier: GPL-2.0-only

project('partfs-progs', 'c',
  version : '0.1.0',
  default_options : [
    'c_std=c11',
    'warning_level=2',
  ]
)

add_project_arguments(
  '-Wall', '-Wextra', '-Werror',
  language : 'c'
)

subdir('mkfs')
subdir('man')

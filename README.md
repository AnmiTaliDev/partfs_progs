# partfs-progs

Userspace tools for **PartFS** — a modern, patent-free filesystem designed as
a replacement for FAT32/vFAT on contemporary hardware.

PartFS is 64-bit, little-endian, extent-based, with CRC32C checksums on every
metadata block, a write-ahead journal, and a B-tree directory index.  It
carries no Microsoft LFN patents.

## Tools

| Binary | Status | Description |
|--------|--------|-------------|
| `mkfs.part` | **done** | Format a device or image file as PartFS |
| `fsck.part` | planned | Check and repair a PartFS volume |
| `mount.part` | planned | FUSE driver — mount a PartFS volume |
| `tune.part` | planned | Adjust volume parameters after formatting |
| `dump.part` | planned | Dump on-disk metadata for debugging |
| `resize.part` | planned | Grow or shrink a PartFS volume |

## Building

Requires: `meson`, `ninja`, a C11 compiler, Linux kernel headers.

```sh
git submodule update --init
meson setup build
ninja -C build
```

Install system-wide:

```sh
ninja -C build install
```

## mkfs.part

```
mkfs.part [-L label] [-j journal_blocks] <device|file>
```

| Option | Default | Description |
|--------|---------|-------------|
| `-L label` | *(none)* | Volume label, up to 32 bytes UTF-8 |
| `-j N` | `1024` | Journal size in blocks (1 block = 4096 bytes) |

The minimum device size depends on the journal: `N + 11` blocks.  For the
default journal that is **1035 blocks (~4 MiB)**.

### Examples

Format a block device:

```sh
mkfs.part -L "data" /dev/sdb
```

Create and format a 256 MiB image file:

```sh
truncate -s 256M disk.img
mkfs.part -L "test" disk.img
```

See `man mkfs.part` for the full reference.

## License

Licensed under GNU GPL 2.0 — see [LICENSE](LICENSE).

# partfs-progs Roadmap

## v0.1 — mkfs (current)

- [x] `mkfs.part` — format a device or image as PartFS
  - Superblock + backup, journal header, block groups, allocation bitmaps
  - Root inode #1 (directory, mode 0755) with `.` and `..` entries
  - CRC32C on all metadata structures
  - RFC 4122 v4 UUID from `/dev/urandom`
  - `fallocate(FALLOC_FL_ZERO_RANGE)` for fast journal zeroing
  - Volume label (`-L`) and configurable journal size (`-j`)
- [x] `lib/crc32c` — software CRC32C (Castagnoli) with lookup table
- [x] Implementation specification (`PartFS_Implementation_Spec.tex`)

## v0.2 — fsck

`fsck.part` — filesystem checker and repair tool.

- Read and verify superblock CRC32C; fall back to backup if primary is corrupt.
- Walk all block groups: verify group descriptor CRC32C and `block_no`.
- Verify allocation bitmaps against actually reachable blocks.
- Walk inode B-tree: verify node CRC32C, key ordering, `inode_no` self-reference.
- For each inode: verify extent ranges do not overlap and stay within volume bounds.
- Walk directory B-tree leaves: verify record CRC32C, FNV-1a hash consistency,
  `rec_len` alignment.
- Detect and report: orphaned inodes, double-allocated blocks, bad CRC32C,
  incorrect `refcount`, truncated records.
- Repair mode (`-r`): rewrite corrupt structures where recovery is unambiguous.
- Read-only check mode (default): exit code 0 = clean, 1 = errors found, 2 = fatal.

## v0.3 — FUSE driver (mount.part)

`mount.part` — read/write FUSE driver.

- Mount a PartFS volume: `mount.part <device> <mountpoint> [options]`
- Read path: lookup by inode number, extent-based block reads, sparse file holes.
- Write path: journal transactions, bitmap allocation, extent splitting/merging.
- Directory operations: FNV-1a keyed B-tree insert/delete/lookup.
- Per-group inode B-tree creation on first inode allocation in groups 1+.
- Journal replay at mount if `state != clean`.
- Checkpoint (clean unmount): flush journal, set `FLAG_CLEAN`, `fsync`.
- `libfuse3` dependency; no other external libraries.
- Options: `uid=`, `gid=`, `umask=`, `ro` (read-only mount).

## v0.4 — tune and label

`tune.part` — adjust volume parameters on an unmounted volume.

- Change volume label: `tune.part -L <label> <device>`
- Adjust journal size: `tune.part -j <blocks> <device>` (requires free space).
- Set/clear feature flags (xattr, ACL).
- Update superblock and backup, recompute CRC32C.

## v0.5 — dump

`dump.part` — metadata inspection tool for debugging and development.

- `dump.part sb <device>` — print superblock fields in human-readable form.
- `dump.part group <device> [n]` — print group descriptor(s).
- `dump.part inode <device> <ino>` — print inode fields and extents.
- `dump.part dir <device> <ino>` — list directory entries with hashes.
- `dump.part bitmap <device> [group]` — print allocation bitmap as a map.
- `dump.part journal <device>` — print journal header and committed transactions.
- Output formats: human-readable (default), JSON (`--json`), raw hex (`--hex`).
- Useful for verifying `mkfs.part` output and driver development.

## v0.6 — resize

`resize.part` — online or offline volume resize.

- Grow: `resize.part <device> [new_size]` — add new block groups.
- Shrink: `resize.part -s <new_size> <device>` — relocate data, remove tail groups.
- Shrink requires that the tail groups are empty or can be evacuated.
- Update `block_count`, `free_blocks`, and group descriptors; recompute CRC32C.

## Future / Unscheduled

- `mkpart.part` — partition a raw image into PartFS subvolumes (like LVM thin).
- xattr and POSIX ACL support in the FUSE driver (`v0.3` lays the groundwork).
- Inline data for small files (≤ ~2 KiB stored in `inode.tail`).
- Hard link and symlink support in the FUSE driver.
- `O_DIRECT` and `mmap` support via the FUSE driver's `direct_io` flag.
- Kernel module (long-term): native `struct file_operations` implementation.

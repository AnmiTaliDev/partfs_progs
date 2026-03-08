# partfs-progs Roadmap

## v0.1 — done

- [x] `mkfs.part` — format a block device or image file as PartFS
- [x] `-L`, `-j`, `--version` flags
- [x] `man/mkfs.part.8` manual page

## v0.2

- [ ] `fsck.part` — filesystem checker: superblock, groups, bitmaps, inodes, directories
- [ ] `fsck.part -r` — repair mode
- [ ] `dump.part sb` — print superblock fields
- [ ] `dump.part group` — print group descriptors

## v0.3

- [ ] `mount.part` — read/write FUSE 3 driver
- [ ] `mount.part -o ro,uid=,gid=,umask=,allow_other` mount options
- [ ] Journal replay on dirty mount; clean unmount checkpoint
- [ ] `dump.part inode` — print inode fields and extents
- [ ] `dump.part dir` — list directory entries

## v0.4

- [ ] `tune.part -L` — change volume label
- [ ] `tune.part -U random` — regenerate UUID
- [ ] `tune.part -j` — resize journal
- [ ] `tune.part +xattr / +acl` — enable feature flags
- [ ] `dump.part bitmap` — print allocation bitmap
- [ ] `dump.part journal` — print journal state and transactions
- [ ] `dump.part --json` — machine-readable output

## v0.5

- [ ] `resize.part` — grow volume by appending block groups
- [ ] `resize.part -s` — shrink: evacuate tail groups or refuse
- [ ] `resize.part --dry-run`
- [ ] `label.part` — quick label read/write utility
- [ ] xattr read/write in `mount.part` via inode tail

## v0.6

- [ ] POSIX ACL support in `mount.part`
- [ ] `mount.part -o noatime,sync` options
- [ ] Inline data for small files (`IFLAG_INLINE`)
- [ ] Symlink support (`PARTFS_ITYPE_SYMLINK`)
- [ ] Hard link support; `fsck.part` orphan detection

## Future

- Per-inode locking in libpartfs (replace global mutex)
- `O_DIRECT` via FUSE `direct_io`
- `mkpart.part` — subvolume partitioning on a raw image

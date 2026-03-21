#!/usr/bin/env python3
import os
import stat
import sys
from pathlib import Path


def align4(value: int) -> int:
    return (value + 3) & ~3


def write_entry(out, name: str, mode: int, data: bytes) -> None:
    namesz = len(name) + 1
    header = (
        "070701"
        f"{0:08x}"  #ino
        f"{mode:08x}"
        f"{0:08x}"  #uid
        f"{0:08x}"  #gid
        f"{1:08x}"  #nlink
        f"{0:08x}"  #mtime
        f"{len(data):08x}"
        f"{0:08x}"  #devmajor
        f"{0:08x}"  #devminor
        f"{0:08x}"  #rdevmajor
        f"{0:08x}"  #rdevminor
        f"{namesz:08x}"
        f"{0:08x}"  #cek
    ).encode("ascii")
    out.write(header)
    out.write(name.encode("utf-8") + b"\x00")
    out.write(b"\x00" * (align4(110 + namesz) - (110 + namesz)))
    out.write(data)
    out.write(b"\x00" * (align4(len(data)) - len(data)))


def emit_tree(root: Path, out) -> None:
    write_entry(out, ".", stat.S_IFDIR | 0o755, b"")
    for path in sorted(root.rglob("*")):
      relative = path.relative_to(root).as_posix()
      if path.is_dir():
          write_entry(out, relative, stat.S_IFDIR | 0o755, b"")
      else:
          with path.open("rb") as f:
              data = f.read()
          write_entry(out, relative, stat.S_IFREG | 0o755, data)
    write_entry(out, "TRAILER!!!", 0, b"")


def main() -> int:
    if len(sys.argv) != 3:
        print("contoh: mkinitramfs.py <rootfs_dir> <output>", file=sys.stderr)
        return 1

    root = Path(sys.argv[1]).resolve()
    output = Path(sys.argv[2]).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as out:
        emit_tree(root, out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

"""Check that a directory looks like an extracted Turok: Evolution (USA) disc.

    py -3 scripts/verify-dump.py <extracted dir>

Checks that the files are there and that the XBE's title ID matches. It does
not hash the data, so a modified dump passes; it is here to catch a wrong game
or a half-extracted folder before the pipeline spends minutes on it.
"""
import os
import struct
import sys

TITLE_ID = 0x41430004
REQUIRED = ["default.xbe"] + ["supertree%d.tre" % i for i in range(6)]


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    root = argv[1]
    missing = [f for f in REQUIRED if not os.path.isfile(os.path.join(root, f))]
    if missing:
        print("missing from %s: %s" % (root, ", ".join(missing)), file=sys.stderr)
        return 1
    with open(os.path.join(root, "default.xbe"), "rb") as f:
        hdr = f.read(0x1000)
    if hdr[:4] != b"XBEH":
        print("default.xbe is not an XBE", file=sys.stderr)
        return 1
    base, = struct.unpack_from("<I", hdr, 0x104)
    cert_va, = struct.unpack_from("<I", hdr, 0x118)
    off = cert_va - base
    tid, = struct.unpack_from("<I", hdr, off + 0x08)
    name = hdr[off + 0x0C: off + 0x0C + 80].decode("utf-16-le", "replace").split("\0")[0]
    if tid != TITLE_ID:
        print("title ID 0x%08X (%s) is not Turok: Evolution USA (0x%08X)"
              % (tid, name, TITLE_ID), file=sys.stderr)
        return 1
    print("OK: %s, title 0x%08X" % (name, tid))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

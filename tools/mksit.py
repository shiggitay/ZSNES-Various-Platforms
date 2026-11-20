#!/usr/bin/env python3
"""mksit.py - write a classic StuffIt (1.5, "SIT!") archive.

Usage: mksit.py OUT.sit FOLDERNAME ITEM...
where each ITEM is   NAME=FILE.bin       a MacBinary file (both forks,
                                         type, creator and Finder flags)
                or   NAME=FILE:TYPE:CREA a plain file as the data fork

The archive holds one folder, FOLDERNAME, with the items in it, which is what
StuffIt Expander makes of it: a folder on the desktop. Entries are stored
(method 0) rather than compressed: every StuffIt Expander reads that, and
nothing here needs to know StuffIt's compressors. Layout from the StuffIt
1.5 format as macutils' unsit and The Unarchiver read it; CRCs are CRC-16
as ARC computes it (polynomial 0xA001, reflected, initial 0).
"""
import struct
import sys
import time

MAC_EPOCH = 2082844800  # seconds from 1904 to 1970


def crc16(data, crc=0):
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def pascal63(name):
    raw = name.encode("mac_roman")
    if len(raw) > 31:
        raise SystemExit(f"name longer than 31 characters: {name}")
    return bytes([len(raw)]) + raw + bytes(63 - len(raw))


def header(rmethod, dmethod, name, ftype, creator, flags, rsrc, data):
    now = int(time.time()) + MAC_EPOCH
    h = struct.pack(
        ">BB64s4s4sHIIIIIIHH6s",
        rmethod, dmethod, pascal63(name), ftype, creator, flags,
        now, now,
        len(rsrc), len(data), len(rsrc), len(data),
        crc16(rsrc), crc16(data), bytes(6),
    )
    assert len(h) == 110
    return h + struct.pack(">H", crc16(h))


def macbinary(path):
    b = open(path, "rb").read()
    h = b[:128]
    dlen = int.from_bytes(h[83:87], "big")
    rlen = int.from_bytes(h[87:91], "big")
    data = b[128:128 + dlen]
    roff = 128 + ((dlen + 127) // 128) * 128
    rsrc = b[roff:roff + rlen]
    flags = (h[73] << 8) | h[101]
    return h[65:69], h[69:73], flags, data, rsrc


def main():
    out, folder, items = sys.argv[1], sys.argv[2], sys.argv[3:]
    body = header(32, 32, folder, b"\0\0\0\0", b"\0\0\0\0", 0, b"", b"")
    for item in items:
        name, spec = item.split("=", 1)
        if spec.endswith(".bin"):
            ftype, creator, flags, data, rsrc = macbinary(spec)
        else:
            path, ftype, creator = spec.rsplit(":", 2)
            ftype, creator = ftype.encode("mac_roman"), creator.encode("mac_roman")
            data, rsrc, flags = open(path, "rb").read(), b"", 0
        # The Finder sets these itself; an archive carries none of them.
        flags &= ~(0x0100 | 0x0001)  # hasBeenInited, isOnDesk
        body += header(0, 0, name, ftype, creator, flags, rsrc, data) + rsrc + data
    body += header(33, 33, folder, b"\0\0\0\0", b"\0\0\0\0", 0, b"", b"")
    arc = struct.pack(">4sHI4sB7s", b"SIT!", 1, 22 + len(body), b"rLau", 1, bytes(7))
    open(out, "wb").write(arc + body)


if __name__ == "__main__":
    main()

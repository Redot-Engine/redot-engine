#!/usr/bin/env python3
"""Clears a spurious x86-64 ISA requirement from an ELF's GNU property note.

The Flow premake build stamps GNU_PROPERTY_X86_ISA_1_NEEDED = x86-64-v2 onto
libnvflow.so / libnvflowext.so unconditionally -- the marker comes from the
build, not from the code's actual instruction use. glibc's loader refuses
objects whose ISA note exceeds the running CPU ("CPU ISA level is lower than
required") at exec and dlopen, so a blanket marker locks engines out of CPUs
that can actually run every instruction in them.

This tool rewrites the ISA bitmap of the X86_ISA_1_NEEDED property in place
(the note keeps its size) to --level bits, then verifies the rewritten bytes
before writing the file.

Property entry layout (what glibc's loader walks in elf/dl-load.c):
    pr_type (4 bytes), pr_datasz (4 bytes), pr_data[pr_datasz]
with pr_data rounded up to an 8-byte boundary in 64-bit objects. An earlier
version of this tool patched the pr_datasz field by mistake; this version
also repairs such malformed notes (glibc skips any property whose datasz is
not 4, so they gate nothing, but they are not the note we meant to write).
"""

import argparse
import os
import struct

NT_GNU_PROPERTY_TYPE_0 = 5
GNU_PROPERTY_X86_ISA_1_NEEDED = 0xC0000002

ISA_LEVEL_BITS = {
    1: 0x1,  # baseline x86-64 (SSE2)
    2: 0x3,  # v2: + SSE4.2, POPCNT, CMPXCHG16B
    3: 0x7,  # v3: + AVX2, FMA, BMI, LZCNT, MOVBE, F16C
    4: 0xF,  # v4: + AVX-512
}


def sections(data):
    e_shoff = struct.unpack_from("<Q", data, 0x28)[0]
    e_shentsize = struct.unpack_from("<H", data, 0x3A)[0]
    e_shnum = struct.unpack_from("<H", data, 0x3C)[0]
    e_shstrndx = struct.unpack_from("<H", data, 0x3E)[0]

    def sh(i):
        off = e_shoff + i * e_shentsize
        name, typ, flags, addr, offset, size, link, info, align, entsize = struct.unpack_from("<IIQQQQIIQQ", data, off)
        return name, offset, size

    shstr_name, shstr_off, shstr_size = sh(e_shstrndx)
    for i in range(e_shnum):
        name, offset, size = sh(i)
        base = shstr_off + name
        n = data[base : data.index(b"\0", base)].decode()
        yield n, offset, size


def fix(path, level, report_only=False):
    want = ISA_LEVEL_BITS[level]
    with open(path, "rb") as f:
        data = bytearray(f.read())
    if data[:4] != b"\x7fELF" or data[4] != 2:
        print(f"{path}: not an ELF64 object, skipped")
        return False

    patched = 0
    seen = 0
    for name, offset, size in sections(data):
        if name != ".note.gnu.property":
            continue
        blob = data[offset : offset + size]
        pos = 0
        while pos + 12 <= len(blob):
            namesz, descsz, ntype = struct.unpack_from("<III", blob, pos)
            pos += 12
            name = blob[pos : pos + namesz]
            pos += (namesz + 3) & ~3
            desc_off = offset + pos
            desc = blob[pos : pos + descsz]
            pos += (descsz + 3) & ~3
            if namesz != 4 or name != b"GNU\x00" or ntype != NT_GNU_PROPERTY_TYPE_0:
                continue
            # Walk variable-length property entries, exactly like glibc:
            # 8-byte header, then pr_data rounded up to 8 bytes on ELF64.
            j = 0
            while j + 8 <= len(desc):
                pr_type, pr_datasz = struct.unpack_from("<II", desc, j)
                data_off = j + 8
                next_j = data_off + ((pr_datasz + 7) & ~7)
                if pr_type == GNU_PROPERTY_X86_ISA_1_NEEDED:
                    seen += 1
                    if pr_datasz != 4:
                        # Malformed: glibc skips the whole note, so it gates
                        # nothing -- but it is the corrupted output of the
                        # pre-fix version of this tool (it patched the
                        # datasz field instead of the bitmap). Rewrite the
                        # entry well-formed at the requested level.
                        if report_only:
                            print(
                                f"{path}: ISA_1_NEEDED MALFORMED (datasz={pr_datasz}, expected 4) -- skipped by glibc"
                            )
                        else:
                            datasz_off = desc_off + j + 4
                            struct.pack_into("<I", data, datasz_off, 4)
                            struct.pack_into("<I", data, desc_off + data_off, want)
                            if (
                                struct.unpack_from("<I", data, datasz_off)[0] != 4
                                or struct.unpack_from("<I", data, desc_off + data_off)[0] != want
                            ):
                                print(
                                    f"{path}: FAILED to repair ISA_1_NEEDED (verification mismatch), file left unchanged"
                                )
                                return False
                            print(f"{path}: ISA_1_NEEDED repaired: datasz {pr_datasz} -> 4, level -> {want:#x}")
                            patched += 1
                    elif report_only:
                        old = struct.unpack_from("<I", desc, data_off)[0]
                        if old > want:
                            print(f"{path}: ISA_1_NEEDED = {old:#x} (ABOVE {want:#x})")
                        else:
                            print(f"{path}: ISA_1_NEEDED = {old:#x}")
                    else:
                        old = struct.unpack_from("<I", desc, data_off)[0]
                        if old == want:
                            print(f"{path}: ISA_1_NEEDED already {want:#x}")
                        else:
                            struct.pack_into("<I", data, desc_off + data_off, want)
                            if struct.unpack_from("<I", data, desc_off + data_off)[0] != want:
                                print(
                                    f"{path}: FAILED to patch ISA_1_NEEDED (verification mismatch), file left unchanged"
                                )
                                return False
                            print(f"{path}: ISA_1_NEEDED {old:#x} -> {want:#x}")
                            patched += 1
                j = next_j
    if report_only:
        if seen == 0:
            print(f"{path}: no ISA requirement declared")
    elif patched:
        with open(path, "wb") as f:
            f.write(data)
    elif seen == 0:
        print(f"{path}: no X86_ISA_1_NEEDED property found")
    return True


def scan_tree(path, level):
    # Walks a shipped build directory and reports every ELF's declared ISA
    # requirement -- the objects glibc's loader would refuse with "CPU ISA
    # level is lower than required" on a weaker CPU.
    flagged = 0
    for root, _, files in os.walk(path):
        for fname in files:
            full = os.path.join(root, fname)
            try:
                with open(full, "rb") as f:
                    magic = f.read(5)
                if magic[:4] != b"\x7fELF" or magic[4:5] != b"\x02":
                    continue
            except OSError:
                continue
            print(f"-- {full}")
            fix(full, level, report_only=True)
            flagged += 1
    if flagged == 0:
        print(f"no ELF64 objects found under {path}")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--level",
        type=int,
        default=1,
        choices=sorted(ISA_LEVEL_BITS),
        help="ISA level to declare (default: 1 = baseline x86-64)",
    )
    ap.add_argument(
        "--report",
        action="store_true",
        help="only report declared ISA requirements (directories are walked recursively); nothing is modified",
    )
    ap.add_argument("files", nargs="+", help="ELF objects (or directories with --report) to process")
    args = ap.parse_args()
    for path in args.files:
        if args.report and os.path.isdir(path):
            scan_tree(path, args.level)
        else:
            fix(path, args.level, report_only=args.report)


if __name__ == "__main__":
    main()

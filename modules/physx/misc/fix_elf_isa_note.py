#!/usr/bin/env python3
"""Clears a spurious x86-64 ISA requirement from an ELF's GNU property note.

glibc's loader refuses objects whose GNU_PROPERTY_X86_ISA_1_NEEDED bitmap
declares a higher ISA level than the running CPU ("CPU ISA level is lower
than required") at exec and dlopen. Build toolchains stamp that requirement
unconditionally (Flow's premake marks x86-64-v2 for libnvflow regardless of
the instructions actually used; NVIDIA's PhysX static libraries and the
ubuntu-latest gcc-15 toolchain mark x86-64-v3 the same way), locking engines
out of CPUs that can run every instruction in the binary.

This tool rewrites the ISA bitmap of the X86_ISA_1_NEEDED property in place
(the note keeps its size) to --level bits, then verifies the rewritten bytes
before writing the file.

GNU property type constants (glibc elf.h, 2.41+ -- note that older binutils/
glibc used 0xc0000002 for X86_ISA_1_NEEDED; the types were renumbered when
X86_FEATURE_2 was folded away, and glibc 2.41's loader already gates on the
new value):
    0xC0000002  GNU_PROPERTY_X86_FEATURE_1_AND  (CET: IBT | SHSTK)
    0xC0008002  GNU_PROPERTY_X86_ISA_1_NEEDED   <-- this tool rewrites it
Property entry layout (what glibc's loader walks in elf/dl-load.c):
    pr_type (4 bytes), pr_datasz (4 bytes), pr_data[pr_datasz]
with pr_data rounded up to an 8-byte boundary in 64-bit objects. An earlier
version of this tool patched the pr_datasz field by mistake; this version
also repairs such malformed notes (glibc skips any property whose datasz is
not 4, so they gate nothing, but they are not the note we meant to write).

The ISA requirement is read from the PT_GNU_PROPERTY program segment (what
the loader itself walks) when the section walk finds no .note.gnu.property
descriptors -- a valid stripped ELF can omit sections (or the section entry)
while keeping the segment.
"""

import argparse
import os
import struct

NT_GNU_PROPERTY_TYPE_0 = 5
PT_GNU_PROPERTY = 0x6474E553
GNU_PROPERTY_X86_ISA_1_NEEDED = 0xC0008002
GNU_PROPERTY_X86_FEATURE_1_AND = 0xC0000002  # CET marker; NOT gated by the ISA check

ISA_LEVEL_BITS = {
    1: 0x1,  # baseline x86-64 (SSE2)
    2: 0x3,  # v2: + SSE4.2, POPCNT, CMPXCHG16B
    3: 0x7,  # v3: + AVX2, FMA, BMI, LZCNT, MOVBE, F16C
    4: 0xF,  # v4: + AVX-512
}


def _sections(data):
    """Yields (name, offset, size) for every section header; empty when the
    file carries no section-header table."""
    e_shoff = struct.unpack_from("<Q", data, 0x28)[0]
    if e_shoff == 0:
        return
    e_shentsize = struct.unpack_from("<H", data, 0x3A)[0]
    e_shnum = struct.unpack_from("<H", data, 0x3C)[0]
    e_shstrndx = struct.unpack_from("<H", data, 0x3E)[0]
    if e_shnum == 0 or e_shstrndx >= e_shnum:
        return

    def sh(i):
        off = e_shoff + i * e_shentsize
        name, typ, flags, addr, offset, size, link, info, align, entsize = struct.unpack_from("<IIQQQQIIQQ", data, off)
        return name, offset, size

    shstr_name, shstr_off, shstr_size = sh(e_shstrndx)
    for i in range(e_shnum):
        name, offset, size = sh(i)
        base = shstr_off + name
        end = data.find(b"\0", base)
        if end == -1:
            continue
        yield data[base:end].decode(), offset, size


def _phdr_gnu_property_notes(data):
    """Yields (abs_offset_of_desc, desc_bytes) for every NT_GNU_PROPERTY_TYPE_0
    note inside a PT_GNU_PROPERTY program segment -- the view the dynamic
    loader itself walks, available even when sections are stripped."""
    e_phoff = struct.unpack_from("<Q", data, 0x20)[0]
    if e_phoff == 0:
        return
    e_phentsize = struct.unpack_from("<H", data, 0x36)[0]
    e_phnum = struct.unpack_from("<H", data, 0x38)[0]
    for i in range(e_phnum):
        o = e_phoff + i * e_phentsize
        p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from(
            "<IIQQQQQQ", data, o
        )
        if p_type != PT_GNU_PROPERTY or p_filesz < 12:
            continue
        raw = data[p_offset : p_offset + p_filesz]
        pos = 0
        while pos + 12 <= len(raw):
            namesz, descsz, ntype = struct.unpack_from("<III", raw, pos)
            body = pos + 12 + ((namesz + 3) & ~3)
            name = raw[pos + 12 : pos + 12 + namesz]
            if ntype == NT_GNU_PROPERTY_TYPE_0 and namesz == 4 and name == b"GNU\x00":
                yield p_offset + body, raw[body : body + descsz]
            pos = body + ((descsz + 3) & ~3)


def _property_notes(data):
    """Collects GNU property note descriptors, preferring the section view and
    falling back to the program-segment view when the section walk found no
    descriptors (stripped binaries). Yields (absolute_desc_offset, desc_bytes)
    pairs."""
    descriptors = []
    for name, offset, size in _sections(data):
        if name != ".note.gnu.property":
            continue
        blob = data[offset : offset + size]
        pos = 0
        while pos + 12 <= len(blob):
            namesz, descsz, ntype = struct.unpack_from("<III", blob, pos)
            pos += 12
            name_bytes = blob[pos : pos + namesz]
            pos += (namesz + 3) & ~3
            desc_off = offset + pos
            descriptors.append((desc_off, bytes(blob[pos : pos + descsz])))
            pos += (descsz + 3) & ~3
            if namesz != 4 or name_bytes != b"GNU\x00" or ntype != NT_GNU_PROPERTY_TYPE_0:
                descriptors.pop()
    if not descriptors:
        # Fall back to the program-segment view when the section walk found
        # nothing but the file may still carry the loader-visible note.
        for desc_off, desc in _phdr_gnu_property_notes(data):
            descriptors.append((desc_off, desc))
    return descriptors


def fix(path, level, report_only=False):
    want = ISA_LEVEL_BITS[level]
    with open(path, "rb") as f:
        data = bytearray(f.read())
    if data[:4] != b"\x7fELF" or data[4] != 2:
        print(f"{path}: not an ELF64 object, skipped")
        return False

    patched = 0
    seen = 0
    for desc_off, desc in _property_notes(data):
        # Walk variable-length property entries, exactly like glibc: 8-byte
        # header, then pr_data rounded up to 8 bytes on ELF64.
        j = 0
        while j + 8 <= len(desc):
            pr_type, pr_datasz = struct.unpack_from("<II", desc, j)
            data_off = j + 8
            next_j = data_off + ((pr_datasz + 7) & ~7)
            if pr_type != GNU_PROPERTY_X86_ISA_1_NEEDED:
                j = next_j
                continue
            seen += 1
            if pr_datasz != 4:
                # Malformed: glibc skips the whole note, so it gates nothing
                # -- but it is the corrupted output of the pre-fix version of
                # this tool (it patched the datasz field instead of the
                # bitmap). Rewrite the entry well-formed at the requested
                # level.
                if report_only:
                    print(f"{path}: ISA_1_NEEDED MALFORMED (datasz={pr_datasz}, expected 4) -- skipped by glibc")
                else:
                    datasz_off = desc_off + j + 4
                    struct.pack_into("<I", data, datasz_off, 4)
                    struct.pack_into("<I", data, desc_off + data_off, want)
                    if (
                        struct.unpack_from("<I", data, datasz_off)[0] != 4
                        or struct.unpack_from("<I", data, desc_off + data_off)[0] != want
                    ):
                        print(f"{path}: FAILED to repair ISA_1_NEEDED (verification mismatch), file left unchanged")
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
                        print(f"{path}: FAILED to patch ISA_1_NEEDED (verification mismatch), file left unchanged")
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
            except OSError:
                continue
            if magic[:4] != b"\x7fELF" or magic[4:5] != b"\x02":
                continue
            print(f"-- {full}")
            try:
                fix(full, level, report_only=True)
            except (struct.error, UnicodeDecodeError, OSError) as e:
                # One malformed ELF must not stop the walk over the rest.
                print(f"{full}: failed to parse ({e})")
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
    # fix() returns False when the patch did NOT happen (not ELF64, or the
    # verification mismatch left the file unchanged). Exit non-zero so a
    # build step can fail loudly instead of shipping an object that still
    # carries the too-high ISA requirement.
    ok = True
    for path in args.files:
        try:
            if args.report and os.path.isdir(path):
                scan_tree(path, args.level)
            elif not fix(path, args.level, report_only=args.report):
                ok = False
        except (struct.error, UnicodeDecodeError, OSError) as e:
            print(f"{path}: failed to parse ({e})")
            ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Produce reproducible structural and string evidence for the IRIX binary."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

from elftools.elf.elffile import ELFFile


ASCII = re.compile(rb"[\x20-\x7e]{4,}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()

    raw = args.binary.read_bytes()
    strings = [
        {"offset": match.start(), "text": match.group().decode("ascii")}
        for match in ASCII.finditer(raw)
    ]

    with args.binary.open("rb") as stream:
        elf = ELFFile(stream)
        header = {key: str(value) for key, value in elf.header.items()}
        sections = []
        for section in elf.iter_sections():
            sections.append(
                {
                    "name": section.name,
                    "type": str(section["sh_type"]),
                    "address": int(section["sh_addr"]),
                    "offset": int(section["sh_offset"]),
                    "size": int(section["sh_size"]),
                    "flags": int(section["sh_flags"]),
                }
            )

        segments = [
            {
                key: int(segment[key]) if isinstance(segment[key], int) else str(segment[key])
                for key in (
                    "p_type",
                    "p_offset",
                    "p_vaddr",
                    "p_filesz",
                    "p_memsz",
                    "p_flags",
                    "p_align",
                )
            }
            for segment in elf.iter_segments()
        ]

        dynamic = []
        dynamic_section = elf.get_section_by_name(".dynamic")
        if dynamic_section:
            for tag in dynamic_section.iter_tags():
                dynamic.append(
                    {
                        "tag": str(tag.entry.d_tag),
                        "value": getattr(tag, "needed", None)
                        or getattr(tag, "soname", None)
                        or getattr(tag, "rpath", None)
                        or str(tag.entry.d_val),
                    }
                )

        liblist = []
        liblist_section = elf.get_section_by_name(".liblist")
        dynstr_section = elf.get_section_by_name(".dynstr")
        if liblist_section and dynstr_section:
            entries = liblist_section.data()
            names = dynstr_section.data()
            for offset in range(0, min(len(entries), 6 * 20), 20):
                name_offset, timestamp, checksum, version, flags = struct.unpack(
                    ">IIIII", entries[offset : offset + 20]
                )
                name_end = names.find(b"\0", name_offset)
                liblist.append(
                    {
                        "name": names[name_offset:name_end].decode("ascii", errors="replace"),
                        "timestamp": timestamp,
                        "checksum": checksum,
                        "version": version,
                        "flags": flags,
                    }
                )

        symbols = []
        for section_name in (".dynsym", ".symtab"):
            section = elf.get_section_by_name(section_name)
            if not section:
                continue
            for symbol in section.iter_symbols():
                symbols.append(
                    {
                        "table": section_name,
                        "name": symbol.name,
                        "value": int(symbol["st_value"]),
                        "size": int(symbol["st_size"]),
                        "bind": str(symbol["st_info"]["bind"]),
                        "type": str(symbol["st_info"]["type"]),
                        "section": str(symbol["st_shndx"]),
                    }
                )

    result = {
        "path": str(args.binary),
        "size": len(raw),
        "sha256": hashlib.sha256(raw).hexdigest(),
        "header": header,
        "sections": sections,
        "segments": segments,
        "dynamic": dynamic,
        "liblist": liblist,
        "symbols": symbols,
        "strings": strings,
    }
    args.out.write_text(json.dumps(result, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()

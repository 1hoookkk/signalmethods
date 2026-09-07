#!/usr/bin/env python3
"""Disassemble a virtual-address range in the big-endian MIPS-II executable."""

from __future__ import annotations

import argparse
from pathlib import Path

from capstone import CS_ARCH_MIPS, CS_MODE_BIG_ENDIAN, CS_MODE_MIPS32, Cs
from elftools.elf.elffile import ELFFile


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("start", type=lambda value: int(value, 0))
    parser.add_argument("end", type=lambda value: int(value, 0))
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()

    with args.binary.open("rb") as stream:
        elf = ELFFile(stream)
        text = elf.get_section_by_name(".text")
        offset = args.start - int(text["sh_addr"])
        code = text.data()[offset : offset + args.end - args.start]
    engine = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 | CS_MODE_BIG_ENDIAN)
    lines = [
        f"{instruction.address:08x}: {instruction.bytes.hex():8s}  {instruction.mnemonic:10s} {instruction.op_str}"
        for instruction in engine.disasm(code, args.start)
    ]
    args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()

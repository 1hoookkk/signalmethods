"""Boot audity_os_2.00.bin (Audity 2000 / Proteus 2000 family OS) under Unicorn
m68k and log every memory write outside the known ColdFire windows.

Memory map (from OS 2.00 boot + chip-select setup):
  0x00000000 - 0x02000000   OS image / RAM window
  0x0100xxxx                DRAM stack window (SP = 0x01010000)
  0x00d00000 / 0x00e00000   external windows (cleared at boot)
  0x20000000 (MBAR)         ColdFire SIM: chip-selects, UART, timers
  0x00600000 (DSP)          DSP mailbox: index = 0x60000e, data = 0x600000 + (i>>8)*2

Usage: python dev/emu_p2k_boot.py [path] [max_instr]
"""
import struct
from collections import defaultdict
import sys

from unicorn import *
from unicorn import m68k_const as MC

DEFAULT_BIN = r"C:\Users\hooki\Downloads\emu_re_artifacts\audity2000\os\audity_os_2.00.bin"
BIN = sys.argv[1] if len(sys.argv) > 1 and sys.argv[1] else DEFAULT_BIN
MAX_INSTR = int(sys.argv[2]) if len(sys.argv) > 2 else 12_000_000

R_PC = MC.UC_M68K_REG_PC
R_SR = MC.UC_M68K_REG_SR

RESET = 0x00000402
DRAM0, DRAM2 = 0x00000000, 0x02000000
FLASH0, FLASH1 = 0x02000000, 0x02020000
MBAR_BASE = 0x20000000
MBAR1 = 0x20000400
DSP_BASE, DSP_END = 0x00600000, 0x00600100

MOVEC_CTL = {0x00: "CACR", 0x801: "VBR", 0xc03: "RAMBAR1", 0xc04: "RAMBAR0", 0xc0f: "MBAR"}
RESET_SIG = bytes([0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x02])


def image_bytes():
    with open(BIN, "rb") as fp:
        data = fp.read()
    off = data.find(RESET_SIG)
    if off < 0:
        raise SystemExit("reset-vector signature not found in file")
    if off > 0:
        print(f"image offset: 0x{off:X}")
    return data[off:]


class Emu:
    def __init__(self):
        self.mu = None
        self.patches = {}
        self.ctl = {}
        self.out_writes = []
        self.mbar_writes = defaultdict(list)
        self.dsp_writes = []
        self.uart_bytes = []
        self.n_instr = 0
        self.loop_skip = 0
        self.stopped = None
        self.skipped = defaultdict(int)
        self.in_isr = False
        self.last_tick = 0
        self.tick_count = 0
        self.tick_isr = 0x000006E0          # TMR0 tick ISR (writes MBAR+0x111, calls RTOS cb)
        self.tick_every = 10 ** 9          # synthetic TMR tick disabled: mailbox is self-driven
        self.tick_after = 700               # only after boot init
        self.mbox_addrs = {
            0x0003e288: "mbox_read_w",
            0x0003e2bc: "mbox_write_w",
            0x0003e2f0: "mbox_write_l",
            0x00007b0:  "mbox_write_w(boot)",
        }
        self.mbox_log = []
        self.mbox_queue = []
        self.mbox_next = 0
        self.mbox_sent = 0
        self.mbox_done = False
        self.mbox_armed = False
        self.mbox_inflight = False
        self.mbox_init_called = False
        self.real_body = self.load_real_body()

    def u16(self, a):
        return struct.unpack(">H", self.mu.mem_read(a, 2))[0]

    def map(self):
        self.mu = Uc(UC_ARCH_M68K, UC_MODE_BIG_ENDIAN)
        try:
            self.mu.reg_write(R_SR, 0x2700)   # supervisor mode: privileged ops legal
        except UcError:
            pass
        img = image_bytes()
        self.mu.mem_map(DRAM0, 0x400000)
        self.mu.mem_write(DRAM0, img)
        for a, b in self.patches.items():
            self.mu.mem_write(a, b)
        self.mu.mem_map(0x01000000, 0x400000)
        self.mu.mem_map(0x00d00000, 0x100000)
        self.mu.mem_map(0x00e00000, 0x100000)
        self.mu.mem_map(FLASH0, 0x20000)
        self.mu.mem_map(0x00400000, 0x400000)   # CS4 window (contains DSP mbox at 0x600000)
        self.mu.mem_map(0x00800000, 0x100000)   # CS5 window
        self.mu.mem_map(0x00f00000, 0x100000)   # CS? window
        self.mu.mem_map(MBAR_BASE, 0x4000)

    def hook_code(self, mu, addr, size, ud):
        self.n_instr += 1
        if self.n_instr > MAX_INSTR:
            self.stopped = ("instr-limit", addr)
            mu.emu_stop()
            return
        w = self.u16(addr)
        if addr in self.mbox_addrs:
            self.log_mbox(addr)
        # Drive the DSP mailbox: first the OS's OWN init routine (FUN 0x3e19c, the
        # four constant-fill loops), then the real 240-byte body through 0x3e2bc.
        if addr == 0x00001604 and self.mbox_inflight:
            self.mbox_inflight = False
        if (not self.mbox_init_called and self.n_instr > 120000):
            self.mbox_init_called = True
            self.inject_os_init(mu)
            return
        if (self.mbox_init_called and not self.mbox_queue and not self.mbox_armed
                and not self.mbox_inflight and not self.mbox_done):
            self.mbox_armed = True
            self.build_mbox_queue()
            return
        if (self.mbox_queue and not self.mbox_done and self.n_instr > 120000
                and (not self.mbox_inflight or self.n_instr - self.mbox_inflight > 300)):
            self.inject_mbox_call(mu)
            return
        if (not self.mbox_armed and not self.mbox_done and self.n_instr > 120000
                and self.n_instr - self.mbox_next > 200000):
            self.mbox_armed = True
            self.build_mbox_queue()
            return
        if self.in_isr and w == 0x4E73:            # rte: pop SR+PC from (SP)
            sp = mu.reg_read(MC.UC_M68K_REG_A7)
            sr_pc = mu.mem_read(sp, 6)
            mu.reg_write(MC.UC_M68K_REG_A7, sp + 6)
            mu.reg_write(R_PC, struct.unpack(">I", sr_pc[2:6])[0])
            self.in_isr = False
            self.tick_count += 1
            return
        # Inject a synthetic TMR0 tick only at the stable idle-loop branch
        # (handled in the busy-wait branch below); nothing else here.
        if w == 0x4E7B:                       # movec (4 bytes)
            self.emu_movec(addr)
            return
        if w == 0x4E73:                       # rte (non-injected)
            mu.reg_write(R_PC, addr + 2)
            return
        if addr == 0x010005B4:                # settle loop
            mu.reg_write(R_PC, 0x010005BC)
            self.loop_skip += 1
            return
        # Privileged SR ops that fault the Unicorn m68k core: NOP-patch in memory
        if (w & 0xF1C0) == 0x46C0:            # move <ea>, SR/CCR (2 bytes)
            mu.mem_write(addr, b"\x4E\x71")
            self.skipped["sr_write"] += 1
            return
        if (w & 0xFF00) == 0x40C0:            # move.w SR, <ea> (2 bytes)
            mu.mem_write(addr, b"\x4E\x71")
            self.skipped["sr_read"] += 1
            return
        if (w & 0xF000) == 0xF000:            # ColdFire MAC / F-line ext: NOP (2 bytes)
            mu.reg_write(R_PC, addr + 2)
            return
        if w == 0x46FC:                       # move #imm, SR (4 bytes)
            mu.mem_write(addr, b"\x4E\x71\x4E\x71")
            self.skipped["sr_imm"] += 1
            return
        if w == 0x4E72:                       # stop #imm (4 bytes)
            mu.mem_write(addr, b"\x4E\x71\x4E\x71")
            self.skipped["stop"] += 1
            return
        # busy-wait: single-instruction back-branch
        if (w & 0xFF00) == 0x6600 or (w & 0xFF00) == 0x6000:
            disp = struct.unpack(">b", self.mu.mem_read(addr + 2, 1))[0]
            tgt = addr + 2 + disp
            if tgt < addr:
                self.loop_skip += 1
                if (not self.in_isr and self.n_instr - self.last_tick > self.tick_every):
                    self.inject_tick(mu, addr)
                    self.last_tick = self.n_instr
                    return
                mu.reg_write(R_PC, addr + 2)
                return
        if w == 0x4E90:                              # jsr (A0) with null callback -> skip
            a0 = mu.reg_read(MC.UC_M68K_REG_A0)
            if a0 == 0:
                mu.reg_write(R_PC, addr + 2)

    def log_mbox(self, addr):
        mu = self.mu
        sp = mu.reg_read(MC.UC_M68K_REG_A7)
        name = self.mbox_addrs.get(addr, "?")
        try:
            param = struct.unpack(">I", mu.mem_read(sp + 4, 4))[0]
        except UcError:
            return
        index = param & 0x1f
        off = (param >> 8) & 0x7e
        rec = (self.n_instr, f"{addr:08X}", name, f"{param:08X}", index, off, 0)
        if "write" in name:
            try:
                rec = (self.n_instr, f"{addr:08X}", name, f"{param:08X}", index, off,
                       struct.unpack(">H", mu.mem_read(sp + 0xA, 2))[0])
            except UcError:
                rec = (self.n_instr, f"{addr:08X}", name, f"{param:08X}", index, off, 0xFFFF)
        self.mbox_log.append(rec)

    def load_real_body(self):
        import pathlib
        p = pathlib.Path(r"C:\Users\hooki\trench-native\ref\presets\P2k_013_talking_hedz.bin")
        if p.exists():
            raw = p.read_bytes()
            words = list(struct.unpack("<120H", raw[:240]))
            return words
        # fallback: silent neutral body
        return [0x7F7] * 120

    def build_mbox_queue(self):
        # The OS encodes a DSP param as: idx = param & 0x1f (-> 0x60000e),
        # data offset = (param>>8)&0x7e (-> 0x600000+off).  The voice upload
        # writes a 240-byte body as a contiguous param block from base 0x2800.
        # Emit the real TalkingHedz body words through that exact encoding.
        words = self.real_body
        self.mbox_queue = [(0x2800 + k, int(w)) for k, w in enumerate(words)]
        self.mbox_next = self.n_instr
        print(f"real body queue prepared: {len(self.mbox_queue)} writes "
              f"(from P2k_013_talking_hedz.bin, 120 words)")

    def inject_os_init(self, mu):
        # Run the OS's real band-init routine FUN 0x3e19c (constant 0x7f7 fills
        # of blocks 0x2800..0x2e00) so its genuine mailbox traffic is logged.
        sp = mu.reg_read(MC.UC_M68K_REG_A7)
        sp -= 4
        mu.mem_write(sp, struct.pack(">I", 0x00001604))   # return: idle loop
        mu.reg_write(MC.UC_M68K_REG_A7, sp)
        mu.reg_write(R_PC, 0x0003e19c)
        self.mbox_inflight = self.n_instr
        print(f"injected OS band-init routine 0x3e19c at instr {self.n_instr}")

    def inject_mbox_call(self, mu):
        if not self.mbox_queue:
            return
        param, data = self.mbox_queue.pop(0)
        self.mbox_sent += 1
        sp = mu.reg_read(MC.UC_M68K_REG_A7)
        sp -= 12
        mu.mem_write(sp + 0, struct.pack(">I", 0x00001604))     # return: idle loop
        mu.mem_write(sp + 4, struct.pack(">I", param))
        mu.mem_write(sp + 8, struct.pack(">HH", 0x0000, data))  # data.w at +0xa
        mu.reg_write(MC.UC_M68K_REG_A7, sp)
        mu.reg_write(R_PC, 0x0003e2bc)                          # mbox write-word
        self.mbox_inflight = self.n_instr
        if not self.mbox_queue:
            self.mbox_done = True

    def inject_tick(self, mu, pc):
        # 68k exception frame: [SR:2][PC:4] pushed below SP, then jump
        sp = mu.reg_read(MC.UC_M68K_REG_A7)
        mu.mem_write(sp - 2, struct.pack(">H", 0x2700))
        mu.mem_write(sp - 6, struct.pack(">I", pc))
        mu.reg_write(MC.UC_M68K_REG_A7, sp - 6)
        mu.reg_write(R_PC, self.tick_isr)
        self.in_isr = True

    def emu_movec(self, addr):
        mu = self.mu
        ow = self.u16(addr + 2)
        ctl = ow & 0x7f
        dn = (ow >> 12) & 7
        src_to_ctl = (ow & 0x8000) == 0
        if src_to_ctl:
            val = mu.reg_read(getattr(MC, f"UC_M68K_REG_D{dn}"))
            self.ctl[ctl] = val
        else:
            val = self.ctl.get(ctl, 0)
            mu.reg_write(getattr(MC, f"UC_M68K_REG_D{dn}"), val)
        mu.reg_write(R_PC, addr + 4)
        name = MOVEC_CTL.get(ctl, f"ctl{ctl:02x}")
        if name == "MBAR":
            self.ctl["MBAR_base"] = val & ~1
        print(f"  movec {name} <- 0x{val:08X} (pc=0x{addr:08X})")

    def hook_write(self, mu, access, addr, size, value, ud):
        if DSP_BASE <= addr < DSP_END:
            self.dsp_writes.append((self.n_instr, addr, size, value))
            return
        if DRAM0 <= addr < DRAM2 or 0x01000000 <= addr < 0x01040000:
            return
        if 0x00400000 <= addr < 0x00c00000 or 0x00d00000 <= addr < 0x01000000:
            return
        if MBAR_BASE <= addr < MBAR1:
            self.mbar_writes[addr].append((self.n_instr, size, value))
            if addr in (MBAR_BASE + 0x144, MBAR_BASE + 0x1C4):
                self.uart_bytes.append((self.n_instr, addr - MBAR_BASE, value & 0xFF))
            return
        self.out_writes.append((addr, size, value))

    def hook_invalid(self, mu, access, intn, addr, size, value, ud):
        pc = mu.reg_read(R_PC)
        return None

    def hook_unmapped(self, mu, access, addr, size, value, ud):
        if access & UC_MEM_WRITE:
            self.out_writes.append((addr, size, value))
            return True
        # read from unmapped: return zero so the OS treats hardware as idle
        self.unmapped_reads.append((self.n_instr, addr, size))
        return True

    def run(self):
        self.unmapped_reads = []
        cur = RESET
        for _ in range(200):
            self.map()                     # fresh engine + patches -> no stale ICACHE
            self.mu.hook_add(UC_HOOK_CODE, self.hook_code)
            self.mu.hook_add(UC_HOOK_MEM_WRITE, self.hook_write)
            self.mu.hook_add(UC_HOOK_MEM_UNMAPPED, self.hook_unmapped)
            try:
                self.mu.emu_start(cur, 0, timeout=0)
                print("ran to end")
                break
            except UcError as e:
                pc = self.mu.reg_read(R_PC)
                try:
                    w = self.u16(pc)
                except UcError:
                    print(f"no code at 0x{pc:08X}: {e}")
                    break
                if (w & 0xF1C0) == 0x46C0 or (w & 0xFF00) == 0x40C0:
                    patch = b"\x4E\x71"
                elif w == 0x46FC or w == 0x4E72:
                    patch = b"\x4E\x71\x4E\x71"
                elif (w & 0xF000) == 0xF000:
                    patch = b"\x4E\x71"
                elif w == 0x4E7B:
                    continue
                else:
                    print(f"unhandled fault opcode 0x{w:04X} at 0x{pc:08X}: {e}")
                    break
                self.patches[pc] = patch
                self.skipped["trap_patch"] += 1
                cur = pc
                print(f"patched 0x{w:04X} at 0x{pc:08X} ({self.skipped['trap_patch']})")
        self.report()

    def report(self):
        print(f"\n=== EMU ({self.n_instr} instr, "
              f"{'stopped@limit' if self.stopped else 'complete'}) ===")
        print(f"-- loop skips: {self.loop_skip}  sr patch: {dict(self.skipped)}")
        ext = self.out_writes
        print(f"\n-- writes OUTSIDE known windows: {len(ext)}")
        for a, s, v in ext[:50]:
            print(f"  -> 0x{a:08X} sz{s} val 0x{v:04X}")
        if len(ext) > 50:
            print(f"  ... +{len(ext)-50} more")
        print(f"\n-- MBAR writes: {len(self.mbar_writes)} distinct regs")
        for a, ws in sorted(self.mbar_writes.items()):
            last = ws[-1]
            print(f"  MBAR+0x{a-MBAR_BASE:03X}: {len(ws)} writes, last(instr {last[0]}) val={last[2]}")
        print(f"\n-- UART TX bytes: {len(self.uart_bytes)}")
        print(f"\n-- unmapped reads (papered over): {len(self.unmapped_reads)}")
        for n, a, s in self.unmapped_reads[:30]:
            print(f"  instr {n} read 0x{a:08X} sz{s}")
        if len(self.unmapped_reads) > 30:
            print(f"  ... +{len(self.unmapped_reads)-30} more")
        print(f"\n-- TMR ticks injected: {self.tick_count}")
        print(f"-- mbox calls driven: {self.mbox_sent}")
        print(f"\n-- DSP mailbox calls ({len(self.mbox_log)}):")
        for rec in self.mbox_log[:400]:
            if len(rec) == 7:
                n, a, name, param, idx, off, d = rec
                print(f"  instr {n} @{a} {name} param={param} idx={idx} off={off*2:04X} data={d}")
            else:
                print(f"  [short rec] {rec}")
        if len(self.mbox_log) > 120:
            print(f"  ... +{len(self.mbox_log)-120} more")
        print(f"\n-- DSP mailbox writes: {len(self.dsp_writes)}")
        for n, a, s, v in self.dsp_writes[:120]:
            print(f"  instr {n} -> 0x{a:08X} sz{s} val 0x{v:04X}")
        if len(self.dsp_writes) > 120:
            print(f"  ... +{len(self.dsp_writes)-120} more")


if __name__ == "__main__":
    Emu().run()
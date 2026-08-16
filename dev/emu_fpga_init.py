import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

# Let's inspect the instructions and extract the exact FMC command writes in 0x0803919A..0x08039446
import unicorn
from unicorn.arm_const import *

mu = unicorn.Uc(unicorn.UC_ARCH_ARM, unicorn.UC_MODE_THUMB)

# Map memory: Flash, RAM, FMC
mu.mem_map(0x08020000, 0x40000)
mu.mem_write(0x08020000, app_bin)

mu.mem_map(0x20000000, 0x20000)
mu.mem_map(0x40000000, 0x40000)
mu.mem_map(0x60000000, 0x100000)

writes = []
def hook_mem_write(uc, access, address, size, value, user_data):
    if 0x60000000 <= address < 0x60100000:
        writes.append((address, size, value))

mu.hook_add(unicorn.UC_HOOK_MEM_WRITE, hook_mem_write)

# Set SP, R0, etc.
mu.reg_write(UC_ARM_REG_SP, 0x20010000)
# Start emulation at 0x0803919A (in Thumb mode, so + 1)
# Hook function calls (bl) to just skip
def hook_code(uc, address, size, user_data):
    if address in (0x08039430, 0x0803943A, 0x08039440):
        # skip BL
        uc.reg_write(UC_ARM_REG_PC, address + size | 1)

mu.hook_add(unicorn.UC_HOOK_CODE, hook_code)

mu.emu_start(0x0803919A | 1, 0x08039446)

print("=== EXACT FMC WRITES DURING FPGA INIT ===")
for addr, sz, val in writes:
    target = "CMD (0x60000000)" if addr == 0x60000000 else ("DATA (0x60040000)" if addr == 0x60040000 else f"0x{addr:08X}")
    print(f"Write to {target:20s}: 0x{val:02X} ({val:3d})")


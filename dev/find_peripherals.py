import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

# STM32F4 Memory Map:
# FMC Bank 1: 0x60000000 - 0x6FFFFFFF (OLED is at Bank 1 NOR/SRAM 1: 0x60000000)
# FMC Bank 2: 0x70000000 (NAND)
# FMC Bank 3: 0x80000000 (NAND)
# FMC Bank 4: 0x90000000 (PC Card)
# FMC SDRAM: 0xC0000000 - 0xDFFFFFFF
# Peripherals:
# SPI1: 0x40013000, SPI2: 0x40003800, SPI3: 0x40003C00, SPI4: 0x40013400, SPI5: 0x40015000, SPI6: 0x40015400
# SAI1: 0x40015800, SAI2: 0x40015C00
# I2S / DMA: 0x40026000 (DMA1), 0x40026400 (DMA2)
# GPIOA..GPIOK: 0x40020000..0x40022800

print("Searching for hardware peripheral pointers...")
periphs = {}
for i in range(0, len(app_bin) - 4, 4):
    w = struct.unpack('<I', app_bin[i:i+4])[0]
    # Check if in peripheral range or memory banks
    if (0x40000000 <= w <= 0x4002FFFF) or (0x40010000 <= w <= 0x5FFFFFFF) or (0x60000000 <= w <= 0xE0000000):
        if w not in [0x60000000, 0x60040000]: # exclude OLED for now
            pool_addr = FLASH_BASE + i
            periphs[pool_addr] = w

for p_addr, target in sorted(periphs.items()):
    print(f"Pool at 0x{p_addr:08X} -> 0x{target:08X}")


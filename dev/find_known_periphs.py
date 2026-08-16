import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

# Look for real hardware peripheral register base addresses in STM32F427/429/437/439:
# GPIOA: 0x40020000, GPIOB: 0x40020400, GPIOC: 0x40020800, GPIOD: 0x40020C00, GPIOE: 0x40021000, GPIOF: 0x40021400, GPIOG: 0x40021800, GPIOH: 0x40021C00, GPIOI: 0x40022000
# RCC: 0x40023800
# FLASH_R: 0x40023C00
# DMA1: 0x40026000, DMA2: 0x40026400
# TIM1..TIM14: 0x40010000..0x40014800, 0x40000000..0x40002000
# SPI1..SPI6: 0x40013000, 0x40003800, 0x40003C00, 0x40013400, 0x40015000, 0x40015400
# USART1..6: 0x40011000, 0x40004400, etc.
# I2C1..3: 0x40005400..0x40005C00
# FMC/FSMC control registers: 0xA0000000 (FMC Bank 1 control: 0xA0000000, SDRAM control: 0xA0000140)
# SAI1: 0x40015800, SAI2: 0x40015C00

known_periphs = {
    0x40023800: "RCC",
    0x40023C00: "FLASH_R",
    0x40020000: "GPIOA", 0x40020400: "GPIOB", 0x40020800: "GPIOC", 0x40020C00: "GPIOD",
    0x40021000: "GPIOE", 0x40021400: "GPIOF", 0x40021800: "GPIOG", 0x40021C00: "GPIOH",
    0x40026000: "DMA1", 0x40026400: "DMA2",
    0x40015800: "SAI1", 0x40015C00: "SAI2",
    0x40013000: "SPI1", 0x40003800: "SPI2", 0x40003C00: "SPI3", 0x40013400: "SPI4",
    0x40000000: "TIM2", 0x40000400: "TIM3", 0x40000800: "TIM4", 0x40000C00: "TIM5",
    0x40010000: "TIM1", 0x40010400: "TIM8",
    0x40012000: "ADC1/2/3",
    0x40007000: "PWR",
    0xA0000000: "FMC_BCR1",
    0x60000000: "FMC_BANK1_NOR1",
    0x64000000: "FMC_BANK1_NOR2",
    0x68000000: "FMC_BANK1_NOR3",
    0x6C000000: "FMC_BANK1_NOR4",
}

print("Searching for known peripherals in literal pools...")
found = []
for i in range(0, len(app_bin) - 4, 4):
    w = struct.unpack('<I', app_bin[i:i+4])[0]
    for p_addr, name in known_periphs.items():
        if p_addr <= w < p_addr + 0x400:
            pool_addr = FLASH_BASE + i
            found.append((pool_addr, name, w))

for pool_addr, name, val in sorted(found):
    print(f"Pool at 0x{pool_addr:08X} -> {name} (0x{val:08X})")


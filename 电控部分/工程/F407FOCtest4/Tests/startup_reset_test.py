"""Execute the real BIN reset path; never preload initialized RAM from the ELF.

Regression: omitting .data copy or .bss zeroing must fail before constructors.
ELF symbols supply addresses only. Execution uses the paired flashable BIN.
This tests Cortex-M startup, not STM32 peripherals, motor control or real power.

Install requirements-startup.txt, then run this file after building both profiles.
Pass --elf PATH (repeatable) to test another ELF and its same-stem BIN.
"""

import argparse
from pathlib import Path
import struct
import unittest

from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_HOOK_CODE, UC_MODE_MCLASS, UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_XPSR

ROOT = Path(__file__).resolve().parents[1]
FLASH = 0x08000000
FLASH_SIZE = 512 * 1024
RAM = 0x20000000
RAM_SIZE = 128 * 1024


class StartupResetTest(unittest.TestCase):
    def __init__(self, elf_path):
        super().__init__("runTest")
        self.elf_path = Path(elf_path)

    def shortDescription(self):
        return str(self.elf_path.relative_to(ROOT)) if self.elf_path.is_relative_to(ROOT) else str(self.elf_path)

    def runTest(self):
        firmware = self.elf_path.with_suffix(".bin").read_bytes()
        with self.elf_path.open("rb") as stream:
            elf = ELFFile(stream)
            self.assertEqual(elf["e_machine"], "EM_ARM")
            symbols = {s.name: s["st_value"] for s in elf.get_section_by_name(".symtab").iter_symbols()}
            for segment in elf.iter_segments():
                if segment["p_type"] == "PT_LOAD" and segment["p_filesz"]:
                    offset = segment["p_paddr"] - FLASH
                    self.assertGreaterEqual(offset, 0, "Unexpected non-FLASH load segment")
                    self.assertEqual(firmware[offset:offset + segment["p_filesz"]], segment.data(),
                                     "ELF and BIN do not describe the same firmware")

        sdata, edata = symbols["_sdata"], symbols["_edata"]
        sbss, ebss = symbols["_sbss"], symbols["_ebss"]
        self.assertTrue(RAM <= sdata < edata <= sbss < ebss < RAM + RAM_SIZE - 64)
        self.assertTrue(all(x % 4 == 0 for x in (sdata, edata, sbss, ebss, symbols["_sidata"])))
        data_offset = symbols["_sidata"] - FLASH
        expected_data = firmware[data_offset:data_offset + edata - sdata]
        self.assertEqual(len(expected_data), edata - sdata)
        expected_bss = bytes(ebss - sbss)
        initial_sp, reset_pc = struct.unpack_from("<II", firmware)
        self.assertEqual(initial_sp, symbols["_estack"])
        self.assertEqual(reset_pc & ~1, symbols["Reset_Handler"] & ~1)
        self.assertEqual(reset_pc & 1, 1)
        self.assertLessEqual(len(firmware), FLASH_SIZE)

        # Zero RAM is not sufficient: it can hide a missing .bss clear. Also
        # exercise nonzero/random-looking power-on state and retained dirty RAM.
        for pattern in (0x00, 0xA5, 0x5A, 0xFF):
            uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
            uc.mem_map(FLASH, FLASH_SIZE)
            uc.mem_map(RAM, RAM_SIZE)
            uc.mem_map(0xE000E000, 0x1000)  # CPACR used by real SystemInit.
            uc.mem_write(FLASH, firmware)
            uc.mem_write(RAM, bytes([pattern]) * RAM_SIZE)
            snapshots = {}

            def on_instruction(cpu, address, size, user_data):
                if address == (symbols["__libc_init_array"] & ~1):
                    snapshots["before_constructors"] = (
                        bytes(cpu.mem_read(sdata, edata - sdata)),
                        bytes(cpu.mem_read(sbss, ebss - sbss)),
                    )
                if address == (symbols["main"] & ~1):
                    snapshots["main_reached"] = True
                    cpu.emu_stop()

            uc.hook_add(UC_HOOK_CODE, on_instruction)
            for boot in ("power_on", "retained_ram_reset"):
                if boot == "retained_ram_reset":
                    # Model application-modified globals surviving an MCU reset.
                    uc.mem_write(sdata, b"\x3C" * (edata - sdata))
                    uc.mem_write(sbss, b"\xC3" * (ebss - sbss))
                guard = bytes(uc.mem_read(ebss, 64))
                snapshots.clear()
                uc.reg_write(UC_ARM_REG_SP, initial_sp)
                uc.reg_write(UC_ARM_REG_XPSR, 0x01000000)
                with self.subTest(pattern=hex(pattern), boot=boot, check="reaches_main"):
                    uc.emu_start(reset_pc, FLASH + FLASH_SIZE, timeout=5_000_000, count=500_000)
                    self.assertTrue(snapshots.get("main_reached"), "Reset path never reached main")
                    self.assertIn("before_constructors", snapshots)
                if "before_constructors" not in snapshots:
                    continue
                actual_data, actual_bss = snapshots["before_constructors"]
                for region, actual, expected in ((".data", actual_data, expected_data),
                                                  (".bss", actual_bss, expected_bss)):
                    with self.subTest(pattern=hex(pattern), boot=boot, check=region):
                        difference = next((i for i, pair in enumerate(zip(actual, expected))
                                           if pair[0] != pair[1]), None)
                        self.assertIsNone(difference,
                                          f"{region} not initialized before constructors; first mismatch offset={difference}")
                with self.subTest(pattern=hex(pattern), boot=boot, check="guard"):
                    self.assertEqual(bytes(uc.mem_read(ebss, 64)), guard,
                                     "Startup wrote past the end of .bss")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, action="append")
    args = parser.parse_args()
    paths = args.elf or [ROOT / "build-gcc" / f"esp32_{profile}" / f"F407FOCtest4_gcc_{profile}.elf"
                        for profile in ("spi", "uart")]
    suite = unittest.TestSuite(StartupResetTest(p.resolve()) for p in paths)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    raise SystemExit(0 if result.wasSuccessful() else 1)

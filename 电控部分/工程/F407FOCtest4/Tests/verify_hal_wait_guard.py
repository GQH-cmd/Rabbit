"""实际HAL等待函数和业务读取函数的主机回归；NVIC/寄存器仅在边界模拟，不访问设备。"""
from pathlib import Path
import argparse
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(text: str, name: str) -> str:
    match = re.search(r"(?:static\s+)?[\w*]+\s+" + re.escape(name) + r"\([^;{}]*\)\s*\{", text)
    if not match:
        raise ValueError(f"Function body missing: {name}")
    start, end, depth = match.start(), match.end(), 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT)
    args = parser.parse_args()
    def read(path: str) -> str:
        candidate = args.source_root / path
        return (candidate if candidate.exists() else ROOT / path).read_text(encoding="utf-8-sig")
    adc, sensor = read("Core/Src/adc.c"), read("Hardware/AS5600.c")
    conf = read("Core/Inc/stm32f4xx_hal_conf.h")
    tick = int(re.search(r"#define\s+TICK_INT_PRIORITY\s+(\d+)U?", conf)[1])
    irq = int(re.search(r"HAL_NVIC_SetPriority\(ADC_IRQn,\s*(\d+)", adc)[1])
    extracted = f"#define SOURCE_TICK_PRIORITY {tick}\n#define SOURCE_ADC_PRIORITY {irq}\n"
    for text, names in (
        (adc, ("ADC_AuxReadFaults", "ADC_AuxLastValue")),
        (sensor, ("AS5600_ReadFaults", "AS5600_ReportedFaults")),
    ):
        for name in names:
            match = re.search(r"^(?:static\s+)?(?:volatile\s+)?uint\w+_t\s+" + name + r"\b[^;\n]*;", text, re.M)
            if match:
                extracted += match[0] + "\n"
            elif name != "ADC_AuxLastValue" and name != "AS5600_ReportedFaults":
                extracted += f"volatile uint8_t {name}=0; /* absent in old driver */\n"
    extracted += "\n".join(line for line in adc.splitlines() if line.startswith("#define ADC_AUX_")) + "\n"
    extracted += function(read("Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c"), "I2C_WaitOnFlagUntilTimeout") + "\n"
    extracted += function(read("Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c"), "HAL_ADC_PollForConversion") + "\n"
    extracted += function(adc, "Read_ADC_Value") + "\n"
    extracted += function(sensor, "AS5600_RdReg") + "\n"
    try:
        extracted += function(sensor, "AS5600_ReportErrors") + "\n"
    except ValueError:
        extracted += "static void AS5600_ReportErrors(void) {} /* no deferred consumer in old driver */\n"
    with tempfile.TemporaryDirectory(prefix="rabbit_wait_guard_") as tmp:
        folder = Path(tmp)
        (folder / "hal_wait_extracted.h").write_text(extracted, encoding="utf-8")
        exe = folder / "hal_wait_guard_test.exe"
        subprocess.run([
            "C:/msys64/mingw64/bin/gcc.exe", "-std=c11", "-O0", "-Wall", "-Wextra", "-Werror",
            "-Wno-unused-parameter", "-ftrivial-auto-var-init=pattern", "-I", str(folder),
            str(ROOT / "Tests/hal_wait_guard_test.c"), "-o", str(exe),
        ], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()

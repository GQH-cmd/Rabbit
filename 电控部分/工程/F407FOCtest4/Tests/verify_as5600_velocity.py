"""运行生产AS5600采样/测速函数；仅替换I2C、时钟和GPIO硬件边界，不连接实物。"""
from pathlib import Path
import argparse
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(text: str, signature: str) -> str:
    start = text.index(signature)
    opening = text.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT)
    args = parser.parse_args()
    source = (args.source_root / "Hardware/AS5600.c").read_text(encoding="utf-8-sig")
    header = (args.source_root / "Hardware/AS5600.h").read_text(encoding="utf-8-sig")
    struct_start = header.index("typedef struct __AS5600STRUCT")
    struct_end = header.index("}AS5600;", struct_start) + len("}AS5600;")
    defines = "\n".join(
        line for line in source.splitlines()
        if line.startswith("#define RAW_TO_") or line.startswith("#define AS5600_VEL_")
    )
    extracted = header[struct_start:struct_end] + "\nextern AS5600 Sensor0,Sensor1;\n" + defines + "\n"
    for name in ("AS5600_ReadFaults", "AS5600_ReportedFaults"):
        match = re.search(r"^(?:static\s+)?(?:volatile\s+)?uint8_t\s+" + name + r"\b[^;\n]*;", source, re.M)
        if match:
            extracted += match[0] + "\n"
    for signature in (
        "void AS5600_Init(",
        "void AS5600_RdRawAngle(",
        "float32_t I2C_Get_Velocity(",
    ):
        extracted += function(source, signature) + "\n"
    with tempfile.TemporaryDirectory(prefix="rabbit_as5600_velocity_") as tmp:
        folder = Path(tmp)
        (folder / "as5600_velocity_extracted.h").write_text(extracted, encoding="utf-8")
        exe = folder / "as5600_velocity_test.exe"
        subprocess.run([
            "C:/msys64/mingw64/bin/gcc.exe", "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-I", str(folder), str(ROOT / "Tests/as5600_velocity_test.c"), "-lm", "-o", str(exe),
        ], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()

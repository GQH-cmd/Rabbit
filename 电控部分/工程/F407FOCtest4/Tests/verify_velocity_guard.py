"""提取固件里的超速判定函数，做主机侧边界测试；不连接电机。"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "Hardware/FOC.c").read_text(encoding="utf-8-sig")


def extract_function(name: str) -> str:
    start = SOURCE.index(f"static uint8_t {name}(")
    opening = SOURCE.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (SOURCE[end] == "{") - (SOURCE[end] == "}")
        end += 1
    return SOURCE[start:end]


def main() -> None:
    defines = "\n".join(
        line for line in SOURCE.splitlines()
        if line.startswith("#define VELOCITY_FAULT_")
    )
    program = f"""
#include <stdint.h>
typedef float float32_t;
{defines}
{extract_function('FOC_IsVelocityUnsafe')}
int main(void) {{
    if (FOC_IsVelocityUnsafe(10.0f, 10.0f/60.0f)) return 1;
    if (!FOC_IsVelocityUnsafe(10.0f, -100.0f/60.0f)) return 2;
    if (!FOC_IsVelocityUnsafe(10.0f, 180.0f/60.0f)) return 3;
    if (FOC_IsVelocityUnsafe(100.0f, 300.0f/60.0f)) return 4;
    if (!FOC_IsVelocityUnsafe(100.0f, 351.0f/60.0f)) return 5;
    if (!FOC_IsVelocityUnsafe(100.0f, -61.0f/60.0f)) return 6;
    if (FOC_IsVelocityUnsafe(0.0f, 0.0f)) return 7;
    return 0;
}}
"""
    with tempfile.TemporaryDirectory(prefix="rabbit_velocity_guard_") as tmp:
        src = Path(tmp) / "test.c"
        exe = Path(tmp) / "test.exe"
        src.write_text(program, encoding="utf-8")
        subprocess.run(["C:/msys64/mingw64/bin/gcc.exe", "-std=c11", "-Wall", "-Wextra", "-Werror", str(src), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    if "PID(&M1_vel_pid, M1_Vels.velocityTar / 60.0f, velocity_feedback) * M1_Motor.dir;" not in SOURCE:
        raise AssertionError("M1 velocity-loop direction regressed")
    print("Velocity guard/sign regression: 8 checks, 0 failures")


if __name__ == "__main__":
    main()

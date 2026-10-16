"""提取实际接触停止代码做主机回归；不连接设备，不启动电机。"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CC = Path("C:/msys64/mingw64/bin/gcc.exe")


def section(text, start, end):
    a = text.index(start)
    return text[a:text.index(end, a)]


def function(text, name):
    match = re.search(r"static void " + name + r"\(void\)\s*\{", text)
    if not match:
        raise ValueError(name)
    depth = 1
    pos = match.end()
    while depth:
        depth += (text[pos] == "{") - (text[pos] == "}")
        pos += 1
    return text[match.start():pos]


def main():
    text = (ROOT / "Core/Src/main.c").read_text(encoding="utf-8-sig")
    motor = (ROOT / "Hardware/Motor.h").read_text(encoding="utf-8-sig")
    defines = "\n".join(re.findall(r"^#define CONTACT_DETECT_.*$", text, re.M))
    code = '#include "MotorAxesConfig.h"\n' + defines + "\n" + section(motor, "// 电机模式", "// 电机参数")
    code += "\nstatic MotorControl M0, M1;\nstatic volatile uint8_t FOC_VelocityFault = 0U;\n"
    code += section(text, "uint8_t M0_EncoderType", "/* USER CODE END PV */")
    code += section(text, "static int32_t App_Scale100(", "static void App_StatusPrint_Task(")
    code += function(text, "App_StatusPrint_Task")
    # 临时目录只包含此次测试生成的源码和可执行文件，退出时自动清理。
    with tempfile.TemporaryDirectory(prefix="rabbit_speed_contact_") as tmp:
        out = Path(tmp)
        (out / "contact_runtime_extracted.h").write_text(code, encoding="utf-8")
        exe = out / "speed_contact_test.exe"
        subprocess.run([str(CC), "-std=c11", "-Wall", "-Wextra", "-Werror", "-DFOC_M1_ONLY=0",
                        "-I", str(out), "-I", str(ROOT / "Hardware"),
                        str(ROOT / "Tests/speed_contact_test.c"),
                        str(ROOT / "Hardware/ContactDetect.c"), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()

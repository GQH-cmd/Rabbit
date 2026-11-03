"""Compile/exercise the production NVS save function without USB or NVS hardware."""
import pathlib
import re
import subprocess
import tempfile

tests = pathlib.Path(__file__).resolve().parent
source = tests.parents[1] / "build-gcc/esp32_spi/esp32_spi_bridge_mqtt_provisioned"
text = (source / "GatewayApp.h").read_text(encoding="utf-8-sig")
start = text.find("static bool reportNetworkSaveFailure(")
if start < 0:
    start = text.index("static bool persistConfig(")
end = text.index("static bool clearConfig(", start)
code = text[start:end]
if re.search(r"\bnvs_(?:flash_erase|erase_all|erase_key|flash_init|flash_deinit)\s*\(", code):
    raise SystemExit("FAIL: network save must not erase/reinitialize NVS")
with tempfile.TemporaryDirectory(prefix="rabbit_network_save_") as output:
    output = pathlib.Path(output)
    (output / "network_save_extracted.h").write_text(code, encoding="utf-8")
    binary = output / "network_save_test.exe"
    subprocess.run([
        r"C:\msys64\mingw64\bin\g++.exe", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-Wno-unused-function", "-I", str(source), "-I", str(output),
        str(tests / "network_save_test.cpp"), "-o", str(binary)
    ], check=True)
    result = subprocess.run([str(binary)], check=False)
    raise SystemExit(result.returncode)

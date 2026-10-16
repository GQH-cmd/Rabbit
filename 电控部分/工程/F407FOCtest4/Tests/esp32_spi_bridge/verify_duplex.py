"""提取两端实际函数做回归；不连接串口、不烧录、不发送运动命令。"""
from pathlib import Path
import subprocess
import hashlib
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = Path('E:/RabbitBuild/host-tests')
CPP = Path('C:/msys64/mingw64/bin/g++.exe')
CC = CPP.with_name('gcc.exe')
ESP = ROOT / 'build-gcc/esp32_spi/esp32_spi_bridge_mqtt_provisioned'
OUT.mkdir(parents=True, exist_ok=True)

def run(*args):
    subprocess.run([str(a) for a in args], check=True, cwd=ROOT)

def section(text, start, end):
    a = text.index(start)
    return text[a:text.index(end, a)]

canonical = ROOT / 'Hardware/FocLinkProtocol.h'
assert canonical.read_bytes() == (ESP/'FocLinkProtocol.h').read_bytes(), '两端协议不一致'
main = (ROOT/'Core/Src/main.c').read_text(encoding='utf-8-sig')
isr = (ROOT/'Core/Src/stm32f4xx_it.c').read_text(encoding='utf-8-sig')
required_contact_parameter_comments = (
    '接触判定采样周期',
    '启动忽略时间',
    '统一接触停止转速阈值',
    '下放和抬起使用同一套判断',
)
assert all(marker in main for marker in required_contact_parameter_comments), \
    'main.c 顶部缺少统一65 RPM接触停止参数的中文说明'
extracted = section(isr, 'uint8_t Parse_Command(', '/* USER CODE END 1 */')
extracted += section(main, 'static void App_USART1_CommandTask(', 'static void App_USART2_CommandTask(')
extracted += section(main, 'static int32_t App_LinkScale(', '#endif')
extracted += section(main, 'void HAL_SPI_TxRxCpltCallback(', '#endif')
(OUT/'stm32_runtime_extracted.h').write_text(extracted, encoding='utf-8')

for name in ('duplex_test', 'stm32_runtime_test'):
    exe = OUT/(name+'.exe')
    run(CPP, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-DFOC_M1_ONLY=0', '-I', ROOT/'Hardware',
        '-I', ESP, '-I', OUT, HERE/(name+'.cpp'), '-o', exe)
    run(exe)

# 共享头文件必须同时兼容 STM32 C11，而不是仅通过 C++ 测试。
c = OUT/'protocol_c_test.c'
c.write_text('#include "FocLinkProtocol.h"\nint main(void){uint8_t f[FLP_SIZE];FlpRequest q;'
             'return !(flp_request(f,FLP_COMMAND,1,"PING",0)&&flp_read_request(f,&q));}\n')
run(CC, '-std=c11', '-Wall', '-Wextra', '-Werror', '-I', ROOT/'Hardware', c, '-o', OUT/'protocol_c_test.exe')
run(OUT/'protocol_c_test.exe')
run(CC, '-std=c11', '-Wall', '-Wextra', '-Werror', ROOT/'Tests/contact_detect_test.c',
    ROOT/'Hardware/ContactDetect.c', '-o', OUT/'contact_test.exe')
run(OUT/'contact_test.exe')

# 与改动前备份比较接触判定整个实现，不能只检查一个宏。
backup = ROOT/'build-gcc/esp32_spi/backups/before_duplex_20260920_133253/Core_Src_main.c'
if backup.exists():
    old = backup.read_text(encoding='utf-8-sig')
    assert section(main,'static int32_t App_Scale100(', 'static void App_StatusPrint_Task(') == section(
        old,'static int32_t App_Scale100(', 'static void App_StatusPrint_Task('), '接触检测被意外修改'
    def contact_defines(text):
        return [(name, value.strip()) for name, value in
                re.findall(r'^#define\s+(CONTACT_DETECT_\S+)\s+([^/\r\n]+)', text, re.M)]
    expected = dict(contact_defines(old))
    # 有意修复v1/v5绕过接触保护；其余参数（尤其65 RPM及启动忽略期）保持原值。
    expected['CONTACT_DETECT_MIN_SPEED_TARGET'] = '0.0f'
    assert dict(contact_defines(main)) == expected, '接触检测出现超出低速漏判修复范围的参数变化'
    print('PASS contact task unchanged; speed gate fixed; STOP_RPM=65')
else:
    assert re.search(r'#define CONTACT_DETECT_STOP_RPM\s+65', main), 'STOP_RPM 不是 65'
    print('INFO 未随仓库提交本地备份，已通过源码断言确认 STOP_RPM=65')
print('PASS C11 protocol, four contact cases and protocol header identity')
print('Protocol SHA256:', hashlib.sha256(canonical.read_bytes()).hexdigest())
run(sys.executable, ROOT/'Tests/verify_speed_contact.py')

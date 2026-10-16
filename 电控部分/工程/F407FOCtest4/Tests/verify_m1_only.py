"""单M1行为回归：运行实际协议/解析/采样/校准/网关函数，不接设备。"""
from pathlib import Path
import argparse,re,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
def function(text,name):
    m=re.search(r'(?:static\s+)?[\w*]+\s+'+re.escape(name)+r'\([^;{}]*\)\s*\{',text)
    if not m:raise ValueError(name)
    pos=m.end();depth=1
    while depth:
        depth+=(text[pos]=='{')-(text[pos]=='}');pos+=1
    return text[m.start():pos]
def section(text,start,end):
    a=text.index(start);return text[a:text.index(end,a)]
def main():
    p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,default=ROOT);p.add_argument('--part',choices=['feed','stm32','gateway','control','all'],default='all');a=p.parse_args()
    src=a.source_root;g=src/'build-gcc/esp32_spi/esp32_spi_bridge_mqtt_provisioned';here=ROOT/'Tests/esp32_spi_bridge'
    cc='C:/msys64/mingw64/bin/gcc.exe';cpp='C:/msys64/mingw64/bin/g++.exe'
    with tempfile.TemporaryDirectory(prefix='rabbit_m1_only_') as tmp:
        out=Path(tmp);main=(src/'Core/Src/main.c').read_text(encoding='utf-8-sig');isr=(src/'Core/Src/stm32f4xx_it.c').read_text(encoding='utf-8-sig')
        motor=(src/'Hardware/Motor.c').read_text(encoding='utf-8-sig');foc=(src/'Hardware/FOC.c').read_text(encoding='utf-8-sig')
        config=src/'Hardware/MotorAxesConfig.h'
        prefix='#include "MotorAxesConfig.h"\n' if config.exists() else ''
        code=prefix+section(isr,'uint8_t Parse_Command(','/* USER CODE END 1 */')
        code+=function(main,'App_DiscardPendingMotorCommands')+'\n'
        code+=section(main,'static void App_USART1_CommandTask(','static void App_USART2_CommandTask(')
        code+=section(main,'static int32_t App_LinkScale(','#endif')+section(main,'void HAL_SPI_TxRxCpltCallback(','#endif')
        (out/'stm32_runtime_extracted.h').write_text(code,encoding='utf-8')
        app=(g/'GatewayApp.h').read_text(encoding='utf-8-sig')
        code=section(app,'static void applyRelayOutputs(','static bool fromAp(')+section(app,'static const char* execute(','// 以下对象')+section(app,'static void handleLocal(','static void begin()')
        (out/'gateway_runtime_extracted.h').write_text(code,encoding='utf-8')
        types=(src/'Hardware/Motor.h').read_text(encoding='utf-8-sig')
        (out/'m1_control_types.h').write_text(prefix+section(types,'// 电机模式','extern CURRENT_s')+'\n',encoding='utf-8')
        control=function(motor,'Current_Init')+'\n'
        control+=function(motor,'HAL_ADCEx_InjectedConvCpltCallback')+'\n'
        for name in ('Check_Zero_Angle','Check_Dir','FOC_Init','SetSVPWM'):control+=function(foc,name)+'\n'
        control+=function((src/'Core/Src/tim.c').read_text(encoding='utf-8-sig'),'Motor_TIM18_Init')+'\n'
        (out/'m1_control_extracted.h').write_text(control,encoding='utf-8')
        targets=[('feed',cpp,ROOT/'Tests/m1_only_feed_test.cpp'),('stm32',cpp,here/'m1_only_stm32_test.cpp'),('gateway',cpp,here/'m1_only_gateway_test.cpp'),('control',cc,ROOT/'Tests/m1_only_control_test.c')]
        failures=0
        for part,compiler,file in targets:
            if a.part not in ('all',part):continue
            exe=out/(part+'.exe')
            command=[compiler,'-std='+('c11' if part=='control' else 'c++17'),'-Wall','-Wextra','-Werror','-Wno-unused-parameter','-DFOC_M1_ONLY=1','-I',str(src/'Hardware'),'-I',str(g),'-I',str(out),'-I',str(here/'stubs'),str(file),'-lm','-o',str(exe)]
            subprocess.run(command,check=True);result=subprocess.run([str(exe)]);failures+=result.returncode!=0
        if failures:raise SystemExit(1)
if __name__=='__main__':main()

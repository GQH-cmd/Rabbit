"""运行真实编码器错误、重检、解锁及命令解析函数；只模拟硬件，不连接设备。"""
from pathlib import Path
import argparse,re,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
def function(text,name):
    m=re.search(r'(?:static\s+)?[\w*]+\s+'+name+r'\([^;{}]*\)\s*\{',text)
    if not m:return None
    end=m.end();depth=1
    while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[m.start():end]
def main():
    p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,default=ROOT);a=p.parse_args();src=a.source_root
    foc=(src/'Hardware/FOC.c').read_text(encoding='utf-8-sig');sensor=(src/'Hardware/AS5600.c').read_text(encoding='utf-8-sig')
    motor=(src/'Hardware/Motor.h').read_text(encoding='utf-8-sig');ash=(src/'Hardware/AS5600.h').read_text(encoding='utf-8-sig')
    controller=(src/'Hardware/Controller.h').read_text(encoding='gb18030')
    types=motor[motor.index('// 电机模式'):motor.index('extern CURRENT_s')]
    types+=ash[ash.index('typedef struct __AS5600STRUCT'):ash.index('}AS5600;')+len('}AS5600;')]
    types+=controller[controller.index('typedef struct __PIDSTRUCT'):controller.index('}PIDController;')+len('}PIDController;')]
    code='\n'.join(line for line in sensor.splitlines() if line.startswith('#define RAW_TO_'))+'\n'
    for name in ('AS5600_Init','AS5600_RdReg','AS5600_RdRawAngle'):
        code+=function(sensor,name)+'\n'
    for name in ('FOC_M1EncoderFaultStop','FOC_M1RecoverEncoder','FOC_M1ReleaseRecoveryHold'):
        body=function(foc,name)
        # Old firmware has no recovery entry. Model that absent entry as rejecting,
        # so RED is a behavior failure rather than a compiler/linker failure.
        code+=(body if body else ('void '+name+'(void){}' if name.endswith('FaultStop') else 'uint8_t '+name+'(void){return 0;}'))+'\n'
    main_c=(src/'Core/Src/main.c').read_text(encoding='utf-8-sig')
    discard=function(main_c,'App_DiscardPendingMotorCommands')
    code+=(discard.replace('App_DiscardPendingMotorCommands','App_DiscardPendingMotorCommandsActual') if discard else 'void App_DiscardPendingMotorCommandsActual(void){}')+'\n'
    code+=(function(main_c,'App_ShouldDropRecoveryByte') or 'uint8_t App_ShouldDropRecoveryByte(volatile uint8_t *drop,uint8_t ch){return 0;}')+'\n'
    code+=function((src/'Core/Src/stm32f4xx_it.c').read_text(encoding='utf-8-sig'),'Parse_Command')+'\n'
    code+=function((src/'Core/Src/stm32f4xx_it.c').read_text(encoding='utf-8-sig'),'TIM8_BRK_TIM12_IRQHandler')+'\n'
    with tempfile.TemporaryDirectory(prefix='rabbit_encoder_recovery_') as tmp:
        out=Path(tmp);(out/'recovery_types.h').write_text(types,encoding='utf-8');(out/'recovery_extracted.h').write_text(code,encoding='utf-8')
        exe=out/'test.exe'
        subprocess.run(['C:/msys64/mingw64/bin/gcc.exe','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-DFOC_M1_ONLY=1','-I',str(out),'-I',str(src/'Hardware'),str(ROOT/'Tests/encoder_recovery_test.c'),'-lm','-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
if __name__=='__main__':main()

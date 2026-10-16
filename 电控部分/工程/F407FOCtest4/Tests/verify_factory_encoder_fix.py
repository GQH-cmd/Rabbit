"""对照出厂逻辑，确保修复只移除故障时跨总线改脚和单电机降级。"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
angle = (root / "Hardware/AS5600.c").read_text(encoding="utf-8-sig").split(
    "void AS5600_RdRawAngle(", 1
)[1].split("float32_t I2C1_AS5600_GetAngle", 1)[0]
start = (root / "Core/Src/main.c").read_text(encoding="utf-8-sig").split(
    "void Start(void)", 1
)[1].split("// 基于DWT", 1)[0]
assert "HAL_GPIO_Init(GPIOB" not in angle
assert "M0_EncoderType = ENCODER_DISABLED" in angle
assert "M1_EncoderType = ENCODER_DISABLED" in angle
assert start.index("return;") < start.index("FOC_Init();"), "编码器缺失时禁止重新给电机上电"
print("PASS factory-path encoder fix")

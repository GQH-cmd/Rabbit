# 固件说明

| 文件 | 用途 | 状态 |
|---|---|---|
| `F407FOCtest4_original.bin` | 原始恢复/对照固件 | 基线 |
| `F407FOCtest4_stop65_both_linked_500ms.bin` | 最后双电机联动测试版 | 已验证过运行；不是单电机最终版 |

使用 STM32CubeProgrammer 的 **Download** 区域，起始地址填 `0x08000000`，选择文件后点击 `Start Programming`，完成后复位或重新上电。

# UI Design Tokens

## 颜色

| Token | Hex | 用途 |
|---|---|---|
| PrimaryGreen | `#176B5B` | 主面板、主按钮 |
| DeepTableGreen | `#073F36` | 牌桌、深色背景 |
| JadeGreen | `#42A58C` | Focus、成功、亮边 |
| WarmGold | `#D9A441` | 主强调、房主、胡牌 |
| DarkGold | `#8C6422` | 深边框、按压态 |
| CreamWhite | `#F4EEDC` | 牌体、浅面板、主文字 |
| InkBlack | `#18201F` | 深文字、中性底 |
| WarningRed | `#B8463A` | 胡、危险、断线 |
| InfoBlue | `#397DA5` | 信息、杠、网络 |
| DisabledGray | `#6A706D` | 禁用、离线 |

## 几何与阴影

- 圆角：Small 8、Medium 16、Large 24、Dialog 32 px。
- 边框：Thin 2、Normal 4、Focus 6 px。
- SmallShadow：黑色 24%，Y=4，Blur=8。
- DialogShadow：黑色 40%，Y=8，Blur=18。
- ButtonPressedShadow：黑色 28%，Y=2，Blur=4。
- GlowShadow：主题色 45%，Blur=12；Android 以静态边框或简化材质代替。

运行时权威资产为 `/Game/UI/Data/DA_UITheme`，设计基准为 PC 1920×1080、Android 宽屏 2400×1080。

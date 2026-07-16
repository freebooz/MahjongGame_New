# Button Style 规范

按钮类型：PrimaryGold、PrimaryGreen、SecondaryBlue、DangerRed、NeutralDark、TransparentIcon、RoundIcon、MahjongAction，以及 Peng、Gang、Hu、Pass、PlayTile。

每种均具备 Normal、Hovered、Pressed、Disabled。横向按钮推荐 Margin `0.1458`；方形操作按钮推荐 Margin `0.1563`。文字必须由 UMG TextBlock 叠加。

视觉优先级：Hu 使用警示红与米白边；Gang 使用信息蓝与暖金边；Peng 使用主绿；Pass 使用中性深色；PlayTile 使用暖金。Pressed 状态整体下移 5 px，Android 触控可见；Hovered 增加上沿高光，仅 PC 使用。

统一运行时资产：`/Game/UI/Data/DA_ButtonStyles`。

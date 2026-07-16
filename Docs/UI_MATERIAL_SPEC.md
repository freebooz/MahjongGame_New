# UI Material 规范

所有材质使用 `User Interface` Domain、Translucent Blend、低成本参数路径，不读取 SceneTexture。

| Material | 关键参数 | 用途 |
|---|---|---|
| M_UI_GradientPanel | TintColor, Opacity | 面板渐变 |
| M_UI_SoftGlow | TintColor, GlowStrength | 头像/按钮柔光 |
| M_UI_Outline | TintColor, OutlineWidth | Focus 描边 |
| M_UI_Desaturate | Saturation | 离线状态 |
| M_UI_Disabled | TintColor, Opacity | 禁用 |
| M_UI_ProgressFill | TintColor, Progress | 进度条 |
| M_UI_NetworkPulse | TintColor, Pulse | 网络状态 |
| M_UI_TileSelected | TintColor, GlowStrength | 手牌选择 |
| M_UI_BackgroundBlurMask | Opacity | 模糊遮罩掩码 |

实例：MI_UI_GoldGlow、MI_UI_GreenGlow、MI_UI_RedWarning、MI_UI_Disabled、MI_UI_TileSelected。Android 默认关闭时间动画或降低刷新频率。

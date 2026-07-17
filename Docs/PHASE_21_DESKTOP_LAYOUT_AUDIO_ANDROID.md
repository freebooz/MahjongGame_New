# Phase 21：桌面牌桌布局、交互音效与 Android 交付

## 本阶段完成内容

- 参照用户提供的桌面麻将截图，重排四人牌桌 HUD：四方玩家信息、暗牌、弃牌区、副露区、中央局况和底部本家手牌。
- 对手手牌仅显示牌背，避免客户端泄露其他玩家牌面。
- 本家 14 张牌时将最后一张摸牌与其余手牌留出间距，强化当前可出牌提示。
- 为通用按钮、选牌、出牌、碰、杠、胡、过补充独立音效，并接入实际交互入口。
- 完成 Android ASTC Shipping 构建、Cook、Stage、Package 与 Archive。

## 交互音效映射

| 操作 | 资源 | 触发位置 |
| --- | --- | --- |
| 通用按钮点击 | `/Game/UI/Audio/SFX_UI_Click` | 自动生成的普通 UMG 按钮 `PressedSlateSound` |
| 选择手牌 | `/Game/UI/Audio/SFX_Tile_Select` | 第一次点击可操作手牌 |
| 出牌 | `/Game/UI/Audio/SFX_Tile_Play` | 再次点击已选手牌并发送出牌请求 |
| 碰 | `/Game/UI/Audio/SFX_Peng` | 碰操作按钮 |
| 杠 | `/Game/UI/Audio/SFX_Gang` | 杠操作按钮 |
| 胡 | `/Game/UI/Audio/SFX_Hu` | 胡操作按钮 |
| 过 | `/Game/UI/Audio/SFX_Pass` | 过操作按钮 |

音效由 `Scripts/GenerateUISounds.ps1` 以 48 kHz、16-bit、单声道 PCM WAV 可重复生成，源文件位于 `SourceArt/UI/Audio`，导入后的 `SoundWave` 位于 `Content/UI/Audio`。运行时通过 `UMahjongUISoundLibrary` 统一加载和播放；资源缺失时安全跳过，不阻断交互流程。手牌和碰/杠/胡/过按钮不绑定通用点击声，避免与对应的操作音效重复播放。

## Android 构建环境

- Unreal Engine：5.8
- Android SDK：`C:\Users\Freebooz\AppData\Local\Android\Sdk`
- NDK：r27c（27.2.12479018）
- JDK：17
- ABI：arm64-v8a
- 最低 SDK：26
- 目标 SDK：34
- 方向：Landscape
- 包名：`com.freebooz.guiyangmahjong`
- 应用名：`贵阳捉鸡麻将`

项目配置强制 Cook `/Game/UI`，确保界面、中文字体回退资源、纹理和交互音效进入最终 IoStore 容器。

## 交付物

- APK：`Saved/Packages/Phase21Android/Android_ASTC/GuiyangMahjong-Android-Shipping-arm64.apk`
- APK 大小：156,419,865 字节
- APK SHA-256：`8837E871C2CBD411F1C1BC898D025CB346489E97D567F1EF4A63491796D72EF3`
- 1280x720 截图：`Saved/UIReview/Phase21Reference2Audio/1280x720/GameHUD.png`
- 1920x1080 截图：`Saved/UIReview/Phase21Reference2Audio/1920x1080/GameHUD.png`
- 自动化日志：`Saved/Logs/Phase21Automation.log`

## 验证结果

| 验证项 | 结果 |
| --- | --- |
| Win64 Editor 编译 | 通过 |
| UMG 自动生成 | 17/17 通过，0 错误、0 警告 |
| 1280x720 HUD 视觉门禁 | 通过 |
| 1920x1080 HUD 视觉门禁 | 通过 |
| 七类音效资源加载 | 通过 |
| 动作按钮音效绑定自动化测试 | 通过 |
| 全量自动化测试 | 33/33 通过 |
| Android 工具链校验 | `Android VALID r27c` |
| Android ASTC Shipping BuildCookRun | 通过，AutomationTool ExitCode 0 |
| IoStore Cook | 642 个 package 成功入包 |
| APK 元数据 | 包名、中文应用名、SDK、arm64-v8a 均通过 `aapt` 校验 |
| 最终容器音效名称扫描 | 七类音效全部存在 |

视觉门禁指标：

- 1280x720：非黑像素比例 0.9924，平均亮度 65.42，最小边缘比例 0.9984。
- 1920x1080：非黑像素比例 0.9954，平均亮度 65.94，最小边缘比例 0.9984。

## 尚需真机确认

本轮执行 `adb devices -l` 时没有检测到连接设备，因此以下项目不能声明为真机已验证：

- APK 安装与首次启动。
- 不同屏幕刘海、安全区和系统导航栏下的实际显示。
- 多点触控、连续选牌/出牌的触控手感。
- 不同 Android 设备扬声器上的音量、音色与延迟。

连接一台开启 USB 调试的 Android 设备后，可直接安装上述 APK 进行最终冒烟测试。

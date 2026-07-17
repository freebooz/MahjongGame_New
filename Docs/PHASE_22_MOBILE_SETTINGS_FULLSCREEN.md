# Phase 22：移动设置、全屏适配与背景音乐交付

## 本轮完成

- 修复大厅“设置”按钮：加载并打开 `/Game/UI/Dialogs/WBP_Settings`，不再只写日志。
- 设置面板支持背景音乐、操作音效、振动开关，以及音乐/音效音量、恢复默认、退出游戏和关闭。
- Android 使用响应式 DPI：18:9、20:9 手机保持 **1.5** 倍；16:10、3:2、4:3 平板按短边降至约 1.10–1.30 倍，避免内容拥挤。
- 手机前景使用 `Fill` 铺满宽屏；平板前景自动切换 `ScaleToFit` 保持设计比例，背景仍铺满整个屏幕。
- 背景层和前景层均使用 `Fill`，关闭 SafeZone 四边缩进并启用 Android Display Cutout，界面覆盖 20:9 屏幕和刘海区域。
- Android 禁用默认触控摇杆：`DefaultTouchInterface=None`。
- 新增全局循环背景音乐 `BGM_FirstLightParticles`，音乐开关和音量滑块即时生效并写入本地配置。

## 音乐来源与许可

- 曲名：First Light Particles
- 作者：Yoiyami
- 来源：https://opengameart.org/node/182244
- 许可：CC0 1.0 Universal，可用于商业与非商业游戏。
- 原始 WAV、SHA-256 和许可记录保存在 `SourceArt/UI/Audio`。

## 验证证据

- Win64 Editor：编译成功。
- UMG：18/18 生成成功，0 个生成器警告，0 个生成器错误。
- UI 自动化：`Saved/Automation/Phase22FullscreenMusic/index.json`，6/6 成功、0 失败。
- Android Shipping：Cook/Stage/Package 成功，AutomationTool ExitCode 0。
- APK：`Saved/Packages/Phase22Android/Android_ASTC/GuiyangMahjong-Android-Shipping-arm64.apk`。
- 真机：`AUUEUT3B24011382`（MAA-AN10），ADB 覆盖安装返回 `Success`，GameActivity 前台运行。
- 1.5 倍全屏截图：`Saved/UIReview/Phase22Mobile/Scale15Awake.png`。

真机截图确认：背景覆盖左右边缘和刘海侧，未出现触控摇杆；登录页所有关键交互均完整可见。设置面板的资源继承关系、必需控件和退出游戏按钮已由自动化测试验证。

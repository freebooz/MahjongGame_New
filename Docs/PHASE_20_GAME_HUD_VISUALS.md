# 阶段 20：GameHUD 牌面与适配门禁

## 目标

关闭阶段 19 留下的 GameHUD 牌名旋转、信息密度和牌面资产未接线问题，并把超宽屏与中文字体从人工备注升级为可重复的自动门禁。

## 完成内容

- 弃牌与副露不再旋转整个 Widget，所有牌面始终朝向本地玩家；座位方向继续由上、下、左、右四个牌池表达。
- 重新整理 GameHUD 状态栏、四方座位、弃牌池、副露区、操作区和手牌区间距。
- 手牌、弃牌和副露接入已有的 27 张万/条/筒正式牌面纹理；尚无独立牌面的字牌安全回退为中文文字。
- 新增 `MahjongTileVisualLibrary`，按 `F麻将牌.Suit` 和 Rank 生成纹理路径。
- 修正 `DT_TileTextureRegistry.csv` 与生成脚本中的条/筒规则索引顺序，使其与 `FMahjongTile::GetRuleIndex` 的万、条、筒顺序一致。
- 新增 `GuiyangMahjong.UI.TileVisualMapping` 自动化测试，锁定万/条/筒映射和字牌回退行为。
- 截图门禁新增 Left、Right、Top、Bottom 四边非黑覆盖率；任一边低于 10% 即失败，避免超宽截图尺寸正确但存在黑带时误报通过。
- 新增 `Scripts/TestUIFontReadiness.ps1`，直接读取 UE `DroidSansFallback.ttf` 字符映射，检查核心中文字形、DPI 曲线和物理屏幕能力。

## 运行证据

- `GuiyangMahjongEditor Win64 Development`：编译成功。
- UMG 生成：17/17 成功，0 错误，0 警告。
- 中文字体源：87 个核心中文字形，缺失 0。
- DPI：`ShortestSide`、720→0.666、1080→1.0 均通过。
- 最终截图矩阵：10 页面 × 2 分辨率，共 20/20 通过。
- 截图结果：`Saved/UIReview/Phase20/result.json`。
- 字体与显示器结果：`Saved/UIReview/Phase20/readiness.json`。
- GameHUD 复核图：`Saved/UIReview/Phase20GameHUDFinal/1280x720/GameHUD.png`。
- 自动化：32/32 Success。
- 自动化报告：`Saved/Automation/Phase20GameHUDVisuals/index.html` 与 `index.json`。
- 日志扫描：未发现 Fatal、Assertion、Ensure、`LogMahjongUI: Error` 或自动化失败。

## 超宽屏门禁证明

本机有两块 1920×1080 显示器，但单个可见窗口最大宽度仍为 1920。请求 2400×1080 Login 截图时：

- 文件尺寸：2400×1080，匹配请求。
- 总体非黑比例：0.8。
- 左边缘覆盖率：1.0。
- 右边缘覆盖率：0.0。
- 新门禁结果：失败，符合预期。

证据位于 `Saved/UIReview/Phase20UltrawideGate/result.json`。这说明门禁已能阻止“尺寸正确但右侧黑带”的伪通过。

## 环境边界

- `NativeUltrawideAvailable=false`，仍需在单屏宽度至少 2340 的设备上完成真超宽视觉验收。
- 字体源字形和编辑器截图已通过，但 `ShippingCookVerified=false`，仍需检查打包后字体随包和回退行为。
- Android SDK 仍被 UE 5.8 判定为 `INVALID r27c`，APK、真机 SafeZone、触控热区和 ASTC 显存未验证。
- 源码版 UE 的 `UnrealEditor.version` BuildId 会被后台平台校验改写；截图矩阵前已用增量构建同步项目模块 BuildId。后续自动化应保留该同步步骤。

## 结论

GameHUD 的牌名旋转、正式牌面接线、条/筒映射和 16:9 信息密度问题已关闭。下一阶段进入 Android SDK/Shipping Cook 与真实超宽设备验证。

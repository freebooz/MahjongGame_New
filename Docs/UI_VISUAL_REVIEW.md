# UI 视觉审查

状态：资产源图、Unreal 导入、17 个 Widget 接线和 `/Game/Maps/UIReviewMap` 已完成。阶段 19 已完成 10 个产品页面在 1920×1080、1280×720 下的真实 D3D12 截图矩阵；超宽屏、GameHUD 最终排版和打包字体仍未验收。

审查分辨率：1920×1080、1280×720、2400×1080、2340×1080。

页面清单：WBP_Login、WBP_Lobby、WBP_CreateRoomDialog、WBP_JoinRoomDialog、WBP_Room、WBP_RuleConfig、WBP_GameHUD、WBP_Settlement、WBP_ErrorToast、WBP_ReconnectOverlay。

每张截图检查：背景清晰度、颗粒/伪影、文字对比、九宫格变形、按钮四态、图标一致性、SafeZone、宽屏留白、手牌识别、装饰遮挡。截图输出目录约定为 `Saved/UIReview/<Resolution>/<Widget>.png`。

## 2026-07-15 实际运行记录

- `/Game/Maps/UIReviewMap` Game 模式成功加载。
- 日志确认 `WBP_RootHUD` 和 `WBP_Login` 已创建并加入视口。
- 1920×1080 窗口截图：`Saved/UIReview/Login_1920x1080_Windowed.png`，文件存在，但像素全黑，失败。
- 1280×720 可见窗口截图：`Saved/UIReview/Login_1280x720_Visible.png`，文件存在，但像素全黑，失败。
- `-RenderOffScreen` 路径只得到 888×500 黑图，失败。
- 2400×1080、2340×1080 未继续浪费运行时间；须先修复本机 Slate/viewport 捕获路径。

## 2026-07-17 阶段 18 实际运行记录

- 根因：ScaleBox 下的 Canvas 缺少明确设计尺寸，Widget 创建成功但运行时内容被测量为零。
- 修复：加入固定设计尺寸容器，并为小型组件设置独立设计尺寸。
- 自动脚本：`Scripts/RunUIVisualReview.ps1`。
- 结果索引：`Saved/UIReview/Phase18/result.json`。
- Login 1920×1080、1280×720、2400×1080、2340×1080：尺寸匹配、非黑检测通过。
- 仍需处理：超宽屏两侧黑边、正式 Logo、协议区间距，以及其余页面的运行态截图。

## 2026-07-17 阶段 19 实际运行记录

- 新增确定性本地视觉审查路由；仅在非 Shipping 且显式传入 `-UIReviewScreenshot` 时生效。
- 自动脚本：`Scripts/RunUIVisualReviewMatrix.ps1`。
- 结果索引：`Saved/UIReview/Phase19/result.json`。
- Login、Lobby、CreateRoomDialog、JoinRoomDialog、Room、RuleConfig、GameHUD、Settlement、ErrorToast、ReconnectOverlay 均完成 1920×1080 和 1280×720 截图，共 20/20 通过。
- 人工复核确认背景填充、Logo、输入框、按钮、规则摘要、结算和重连布局正常。
- GameHUD 对手弃牌文字旋转与信息密度仍需精修，不计为最终美术验收完成。
- 2400×1080、2340×1080 受本机 1920 宽物理显示区域钳制，保留为真宽屏设备待验证项。
- 详细报告：`Docs/PHASE_19_UI_VISUAL_MATRIX.md`。

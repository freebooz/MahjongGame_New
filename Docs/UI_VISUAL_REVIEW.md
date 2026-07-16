# UI 视觉审查

状态：资产源图、Unreal 导入、13 个现有 Widget 接线和 `/Game/Maps/UIReviewMap` 已完成。运行时 RootHUD/Login 创建成功，但本机自动截图回读为纯黑，因此视觉验收未通过，不能把黑图当作页面通过证据。

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

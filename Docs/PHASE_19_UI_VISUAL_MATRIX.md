# 阶段 19：全页面 UI 视觉矩阵

## 目标

把 UI 审查从单一 Login 页面扩展到完整产品路径，并为每个页面提供可重复、无服务端依赖的确定性运行态数据。视觉审查入口只在非 Shipping 构建且显式传入 `-UIReviewScreenshot` 时启用，不修改账号、房间或服务端状态。

## 完成内容

- `MobileRootHUDWidget` 新增本地视觉审查场景，覆盖 Login、Lobby、CreateRoomDialog、JoinRoomDialog、Room、RuleConfig、GameHUD、Settlement、ErrorToast、ReconnectOverlay。
- GameHUD 注入 14 张手牌、四方座位、弃牌、副露、翻鸡事件和胡/杠/碰操作，验证真实数据绑定路径而非静态空壳。
- `RunUIVisualReview.ps1` 支持按页面和阶段输出截图；新增 `RunUIVisualReviewMatrix.ps1` 汇总页面 × 分辨率矩阵。
- 所有根页面改为“背景 ScaleToFill + 内容 SafeZone/ScaleToFit”双层结构，避免背景跟随内容比例产生非预期留白。
- Login 标识复用现有 `Icon_Chicken` 图标并由 UMG 组合中文标题、副标题，保持现有图标体系，不新增孤立位图风格。
- 修复 JoinRoomDialog 操作按钮折叠、Settlement 信息/按钮拥挤、ReconnectOverlay 留白过大、RuleSummary 字号过小等布局问题。

## 运行证据

- UMG 生成：17/17 成功，0 错误，0 警告。
- 截图矩阵：10 页面 × 2 分辨率，共 20/20 通过。
- 分辨率：1920×1080、1280×720。
- 结果索引：`Saved/UIReview/Phase19/result.json`。
- 自动化：`GuiyangMahjong` 31/31 Success。
- 自动化报告：`Saved/Automation/Phase19UIVisualMatrix/index.html` 与 `index.json`。
- 日志扫描：未发现 Fatal、Assertion、Ensure、`LogMahjongUI: Error` 或自动化失败。

每个截图用例均验证目标尺寸、非黑像素比例、平均亮度和文件有效性；同时人工抽查页面内容、按钮布局、文本可读性与背景填充。

## 尚未关闭的边界

- 当前显示器可见宽度为 1920，2400×1080 和 2340×1080 窗口会被操作系统钳制；此前截图虽满足文件尺寸和非黑阈值，但右侧存在物理屏幕边界造成的黑带，因此本阶段不把超宽屏列为真实适配通过。
- GameHUD 已完整显示手牌、弃牌、副露、鸡牌和操作入口，但对手方向的弃牌文字旋转、局面信息密度仍需单独做最终视觉精修。
- 中文字体仍需在 Shipping Cook 后验证字形是否完整随包。
- 本机 Android SDK 状态无效，Android 真机 SafeZone、DPI、ASTC 显存和触控热区尚未验证。

## 结论

阶段 19 的“全页面运行态视觉覆盖”已完成；PC 16:9 双分辨率截图门禁和核心 UI 逻辑回归均通过。超宽屏、GameHUD 最终排版、打包字体与 Android 真机验证进入下一阶段。

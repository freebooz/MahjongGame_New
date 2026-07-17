# 阶段 18：UI 可见渲染与截图回归

## 根因与修复

- 原 UMG 生成树使用 `SafeZone → ScaleBox → Canvas`，Canvas 没有明确的设计期尺寸。
- 运行日志虽然能证明 RootHUD/Login 已创建，但 ScaleBox 可能把内容测量为零，因此真实 D3D 截图只包含黑色世界背景。
- 生成器现改为 `SafeZone → ScaleBox → SizeBox → Canvas`，全屏页面明确使用 1920×1080 设计尺寸。
- 手牌、弃牌、操作按钮和规则配置使用各自的组件设计尺寸，避免全屏修复把 GameHUD 子组件缩成微小内容。
- 截图入口支持安全的相对子目录、可配置等待时间，并拒绝绝对路径和 `..` 路径穿越。

## 自动审查脚本

新增 `Scripts/RunUIVisualReview.ps1`：

- 使用真实 D3D12 渲染启动 `/Game/Maps/UIReviewMap`，不使用 NullRHI。
- 默认依次捕获 1920×1080、1280×720、2400×1080、2340×1080。
- 检查 PNG 实际尺寸、非黑像素比例、平均亮度和文件大小。
- 任一截图缺失、尺寸不符或仍为黑图时返回非零退出码。
- 结果写入 `Saved/UIReview/Phase18/result.json`，截图按分辨率归档。

运行命令：

```powershell
PowerShell -ExecutionPolicy Bypass -File .\Scripts\RunUIVisualReview.ps1
```

## 实际结果

| 分辨率 | 实际尺寸 | 非黑比例 | 平均亮度 | 结果 |
|---|---:|---:|---:|---|
| 1920×1080 | 1920×1080 | 1.0000 | 69.98 | 通过 |
| 1280×720 | 1280×720 | 1.0000 | 70.18 | 通过 |
| 2400×1080 | 2400×1080 | 0.8000 | 55.98 | 通过 |
| 2340×1080 | 2340×1080 | 0.8205 | 57.37 | 通过 |

- 17/17 Widget 重新编译保存，0 error、0 warning。
- `GuiyangMahjongEditor Win64 Development` 编译成功。
- `GuiyangMahjong` 自动化：31/31 Success。
- 自动化报告：`Saved/Automation/Phase18UIVisualReview/index.html`。

## 目视复核结论

- 登录背景、中文文本、按钮、勾选框和版本信息已真实可见，黑图阻塞已解决。
- 2400×1080 与 2340×1080 保留居中的 16:9 内容安全区，两侧出现黑色留边；后续应实现背景填充、前景安全区不拉伸的双层宽屏策略。
- `Img_GameLogo` 当前仍是金色占位块，需替换为正式透明 Logo。
- 用户协议/隐私政策区域在 1280×720 下较紧凑，需在后续视觉精修中增加间距和更明确的按钮状态。
- 本阶段只完成 Login 四分辨率证据；Lobby、Room、GameHUD、结算和重连弹层仍需同样的运行态截图。

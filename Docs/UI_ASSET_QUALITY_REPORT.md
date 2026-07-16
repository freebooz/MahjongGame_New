# UI 资产质量报告

## 自动检查

- 生成 PNG：146。
- 背景：8/8 为 2560×1440 RGB，无烘焙文字要求已写入生成提示词。
- 透明资源：面板、按钮、控件、头像、图标、牌面均为 RGBA；透明角与 Alpha 数据记录在 JSON。
- 命名、尺寸、SHA-256、通道极值、文件大小：已写入 `SourceArt/UI/Data/ui_asset_inventory.json`。
- 九宫格：中心为纯净拉伸区，Margin 见 `UI_9SLICE_SPEC.md`。
- 麻将牌：万/筒/条各 9，RuleIndex 映射 CSV 已生成。

## 已知限制

- AI 背景的“无随机文字/符号”目前由提示约束与人工目视检查保证，后续截图回归仍需检查。
- 图标是程序化 MVP 版本；正式品牌精修可替换同名源图，不修改 UMG 或注册表。
- Unreal 导入已实际执行：146 Texture、6 DataAsset、9 Material、5 Material Instance；回读 146/146、0 导入设置错误。
- 现有 13 个 P0 Widget 已重建并编译保存：13/13。
- 运行截图捕获产出黑图，视觉验收仍为失败，不计入通过数量。
- `GuiyangMahjong` 自动化回归：10/10 Success。

## 体积

- PNG 源图：24.16 MiB，其中背景 23.95 MiB。
- 全部资源按未压缩 RGBA 同时驻留的理论上限：136.45 MiB。
- 实际页面只应加载当前背景；背景允许流送，小图 NeverStream。Android 必须在 Cook/DeviceProfile 将背景最大边限制到 2048 后重新核算 ASTC 显存。

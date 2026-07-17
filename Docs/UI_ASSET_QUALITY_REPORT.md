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
- 现有 17 个 Widget 已重建并编译保存：17/17；原始强制清单完成 16/16。
- GameHUD 已接入本地座位映射、四方弃牌池、副露、鸡牌事件、当前回合手牌启用和单选交互；牌面内容始终朝向本地玩家。
- ReconnectOverlay 已接入跨地图重连状态、保留窗口倒计时、手动重试和返回连接页。
- 阶段 20 已完成 10 个产品页面在 1920×1080、1280×720 下的 20/20 运行截图矩阵，并加入四边黑带检测。
- 27 张万/条/筒牌面已接入手牌、弃牌和副露；条/筒注册顺序已与规则索引统一，字牌暂以文字回退。
- UE 中文回退字体的 87 个核心字形缺失 0，DPI 配置通过；Shipping Cook 字体仍待验证。
- `GuiyangMahjong` 自动化回归：32/32 Success；阶段 20 日志未发现 Fatal、Assertion、Ensure 或 `LogMahjongUI: Error`。

## 体积

- PNG 源图：24.16 MiB，其中背景 23.95 MiB。
- 全部资源按未压缩 RGBA 同时驻留的理论上限：136.45 MiB。
- 实际页面只应加载当前背景；背景允许流送，小图 NeverStream。Android 必须在 Cook/DeviceProfile 将背景最大边限制到 2048 后重新核算 ASTC 显存。

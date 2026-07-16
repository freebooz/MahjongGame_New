# MCP 能力报告

审计时间：2026-07-15

## 已发现的 MCP Server

- `codex_apps`：提供文档模板、Figma、GitHub、OpenAI Developers、Sites 等插件资源。
- 当前资源模板列表为空。
- 当前没有发现 Unreal、UnrealMotionGraphicsMCP、VibeUE 或 Monolith Server。

## 已发现的 Unreal 工具

可调用工具元数据中按 `Unreal`、`UMG`、`VibeUE`、`Monolith` 检索结果为 0。

结论：没有专用 Unreal Editor MCP 工具可用于当前工程。

## UMG 读写能力

- 专用 UMG MCP：不可用。
- 已采用降级能力：UE 5.8 C++ Editor Commandlet。
- `UGenerateMahjongUICommandlet` 可创建 Widget Blueprint、设置父类、构造 WidgetTree、写入 Canvas Slot/Anchor/Size、按钮样式、九宫格 Brush、编译和保存 `.uasset`。
- 生成前执行 BindWidget 名称与类型校验；二次执行可验证资产可加载和可重复生成。

## Blueprint 读写能力

- MCP Blueprint Graph 工具：不可用。
- C++ Editor API 可创建、编译、保存 Widget Blueprint。
- 当前 Widget 业务事件主要在 C++ `NativeConstruct` 中绑定，不依赖 Blueprint Graph。

## 资源导入能力

- 当前无专用 MCP 纹理导入工具。
- 当前工程也没有正式 PNG/TGA/PSD 视觉切图。
- 后续可通过 C++ Editor ImportTask/TextureFactory 实现并强制设置 `TEXTUREGROUP_UI`、无损压缩策略。

## PIE 与截图能力

- 当前无 Unreal MCP PIE/截图工具。
- 可通过 `UnrealEditor-Cmd.exe`、Automation Framework 和必要的 Editor 测试关卡执行命令行验证。
- 无法在没有地图和完整 UI 启动流程时声称已完成运行截图。

## 自动化测试能力

- UE Automation Framework 可用。
- `UnrealEditor-Cmd.exe -ExecCmds="Automation RunTests ..."` 可输出 HTML/JSON 报告。
- Windows Build.bat 可构建 Editor、Game 和 Server Target。

## 不可用或受限能力

- Unreal/UMG/VibeUE/Monolith MCP：未发现。
- 可视化 Widget 回读截图：当前受限。
- Android SDK：平台验证为 `INVALID r27c`，不能执行可信 APK 构建。
- Dedicated Server 四客户端端到端：当前功能和地图尚未完成，不能验证。

## 本项目采用的工具组合

1. PowerShell/文件系统：工程审计和证据收集。
2. `apply_patch`：安全修改 C++、配置、脚本和文档。
3. UE 5.8 UBT/UHT：C++ 编译。
4. UE 5.8 Editor Commandlet：真实创建、编译、保存、回读 Widget Blueprint。
5. UE Automation Framework：规则、网络安全和 Widget 验证。

该组合属于总需求允许的“第五优先级：C++ Editor 扩展”降级路径。所有报告会明确说明未使用专用 Unreal MCP，不伪造 MCP 操作。

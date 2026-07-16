# MCP / Unreal Editor 操作日志

## 操作 001：能力发现

- 时间：2026-07-15
- MCP Server：当前可用工具集合、`codex_apps`
- Tool：工具元数据检索、MCP Resources/Templates 枚举
- 目标资产：当前开发环境
- 操作类型：只读能力发现
- 输入摘要：检索 Unreal、UMG、VibeUE、Monolith 能力
- 执行结果：未发现专用 Unreal/UMG MCP；资源模板为空
- 回读结果：确认仅能采用 UE C++ Editor 扩展降级路径
- Blueprint 编译结果：不适用
- 是否保存：不适用
- 后续问题：运行画面和精确视觉对比需要完整地图/启动流程或可视化 MCP

## 操作 002：现有 P0 UMG 资产生成与回读

- 时间：2026-07-15
- MCP Server：无；UE 5.8 本地 Editor Commandlet
- Tool：`GenerateMahjongUI` Commandlet
- 目标资产：`/Game/UI` 下 10 个 Widget Blueprint
- 操作类型：创建、修改、编译、保存、二次加载
- 输入摘要：1920x1080 设计坐标、SafeZone、ScaleBox、四态 Button、九宫格 Brush、C++ Parent 和 BindWidget
- 执行结果：10/10 保存成功
- 回读结果：二次运行全部资产可加载并重建
- Blueprint 编译结果：0 error，0 warning
- 是否保存：是
- 后续问题：总需求还有 5 个 P0 Widget 未创建；缺正式视觉切图和运行截图

## 操作 003：当前工程审计

- 时间：2026-07-15
- MCP Server：无
- Tool：PowerShell、ripgrep
- 目标资产：`H:\MahjongGame`
- 操作类型：只读审计
- 输入摘要：工程、Target、模块、Content、Config、Docs、SDK、二进制、测试
- 执行结果：确认项目当前是规则/UI 骨架，不是完整 MVP
- 回读结果：55 个源码文件、10 个 UMG、7 项测试，无地图、Scripts、登录、密码房和完整牌局
- Blueprint 编译结果：现有 10 个 Widget 上一轮验证通过
- 是否保存：仅保存审计文档
- 后续问题：按修复计划逐阶段实现并补充日志

## 操作 004：完整 UI 视觉资产生成、导入与 UMG 接线

- 时间：2026-07-15
- 工具：内置图像生成、Pillow、UE 5.8 Editor Python、GenerateMahjongUI Commandlet
- 源资产：8 背景、11 面板、52 按钮、11 控件、9 头像/座位、24 图标、31 麻将牌，共 146 PNG
- Unreal 资产：146 Texture、6 DataAsset、9 UI Material、5 Material Instance
- 导入回读：146/146，UI 压缩和纹理组 0 错误
- Widget：现有 13 个 P0 Widget 全部重新编译保存，13/13
- 构建：GuiyangMahjongEditor Win64 Development 成功
- 运行：UIReviewMap、RootHUD、Login 均成功启动
- 截图：1920×1080、1280×720 文件生成但像素全黑；视觉验收失败，未伪报通过
- 后续问题：修复 Slate/viewport 截图捕获；补 CreateRoom、JoinRoom、RuleConfig 后再跑完整页面矩阵

## 操作 005：UI 资产交付回归测试

- 时间：2026-07-15
- Win64 Editor 构建：成功
- 自动化：`GuiyangMahjong` 10/10 Success
- 覆盖：游客登录、模拟微信与自动登录、登录持久化安全、牌墙、发牌、胡牌、七对、碰杠、鸡牌、零和结算
- 结论：资产导入和 UMG 视觉接线未破坏现有规则与登录测试

## 操作 006：阶段 0/1 规则基线校正

- 时间：2026-07-15
- 工具：UnrealBuildTool、UnrealEditor-Cmd、Git、Git LFS
- 目标：使新工程规则基线符合 `GuiyangMainstreamV1` 默认 108 张、可选 136 张要求
- 写入：牌集枚举、规则版本字段、规则快照、稳定哈希、牌墙配置入口、自动化测试、Git/LFS 基线
- Editor 构建：`GuiyangMahjongEditor Win64 Development` 成功
- 自动化：`GuiyangMahjong` 12/12 Success
- 环境读回：Win64 SDK 有效；Android SDK 为 `INVALID r27c`，未伪报 Android 可构建
- 旧工程保护：`H:\MahjongGame_Test` 未写入、未移动、未删除

## 操作 007：服务端密码房与网络入口

- 时间：2026-07-16
- 工具：UnrealBuildTool、UnrealEditor-Cmd、Windows CNG、OpenSSL
- 写入：RoomManager、房间规则快照、座位/准备/离开生命周期、GameMode RPC 路由、GameState 公开房间复制
- 密码：PBKDF2-HMAC-SHA256 100,000 次、独立盐、常量时间比较、5 次失败锁定 30 秒
- 故障修复：拒绝 UE 5.8 未实现的通用 SHA-256 路径；Win64 改用系统 CNG
- Editor 构建：成功
- 自动化：`GuiyangMahjong` 14/14 Success
- 发布检查：本机缺少 `gh`，GitHub 发布流程按技能安全边界暂停，等待安装并登录 GitHub CLI

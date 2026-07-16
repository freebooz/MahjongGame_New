# H:\MahjongGame 现有工程审计

审计时间：2026-07-15
审计根目录：`H:\MahjongGame`

> 附加需求文本仍写有 `MahjongGame_Test`，但用户在当前任务中已明确要求不再使用该目录，后续实现和验证均以 `H:\MahjongGame` 为唯一工程根目录。

## 工程识别

| 项目 | 当前值 | 证据/状态 |
|---|---|---|
| `.uproject` | `GuiyangMahjong.uproject` | 文件存在 |
| 产品显示名称 | 贵阳捉鸡麻将 | 工程描述和 UI 文案 |
| Unreal Engine | 5.8.0 源码版 | `F:\UnrealEngine-5.8.0-release\Engine\Build\Build.version` |
| Runtime 模块 | `GuiyangMahjong` | `.uproject`、Build.cs |
| Editor 模块 | `GuiyangMahjongEditorTools` | `.uproject`、Build.cs |
| Editor Target | `GuiyangMahjongEditor` | `Source/GuiyangMahjongEditor.Target.cs` |
| Game/Client Target | `GuiyangMahjong` | `Source/GuiyangMahjong.Target.cs`，类型为 Game |
| Server Target | `GuiyangMahjongServer` | `Source/GuiyangMahjongServer.Target.cs` |
| 默认地图 | 未配置 | `DefaultEngine.ini` 无地图项，Content 无 Map 资产 |
| 默认 GameMode | `AGuiyangMahjongGameMode` | `GlobalDefaultGameMode` 已配置 |
| PlayerController | `AGuiyangMahjongPlayerController` | GameMode 构造函数指定 |
| GameState | `AGuiyangMahjongGameState` | GameMode 构造函数指定 |
| PlayerState | UE 默认 `APlayerState` | 尚无项目自定义类 |
| 插件目录 | 不存在 | 当前无项目级 MCP/游戏插件 |

## 当前目录状态

| 目录 | 文件数 | 结论 |
|---|---:|---|
| Source | 55 | C++ 骨架、规则、10 个 Widget 基类、Editor 生成器 |
| Content | 10 | 仅 10 个 UMG `.uasset` |
| Config | 2 | DefaultEngine、DefaultInput |
| Docs | 1（审计前） | 仅上一阶段 UI P0 交付说明 |
| Scripts | 0/不存在 | 尚无启动、构建、四客户端脚本 |
| Plugins | 0/不存在 | 无项目插件 |

`.git` 是空目录，`git status` 返回“not a git repository”，因此当前没有可用提交历史或版本恢复点。

## 当前 UMG Widget

已存在 10 个真实 `.uasset`：

- WBP_ConnectServer
- WBP_Lobby
- WBP_Room
- WBP_GameHUD
- WBP_HandTile
- WBP_DiscardTile
- WBP_ActionButtonPanel
- WBP_Settlement
- WBP_ErrorToast
- WBP_ReconnectOverlay

总需求要求 15 个 P0 Widget；尚缺：

- WBP_RootHUD
- WBP_Login
- WBP_CreateRoomDialog
- WBP_JoinRoomDialog
- WBP_ConfirmDialog

现有 Lobby、Room 的控件和交互也低于总需求，需要扩展而不是只补数量。

## 当前核心逻辑

已实现：

- 136 张标准牌构造、稳定 UniqueId、中文调试名称。
- 洗牌、摸牌和基础发牌测试。
- 基础 3N+2、七对开关。
- 碰、明杠、暗杠、补杠静态判定。
- 基础鸡、翻鸡索引、计鸡。
- 基础结算守恒测试。
- GameState 公共状态 Replication 骨架。
- PlayerController 私有手牌、操作、结算、错误 Client RPC 骨架。

未实现或未闭环：

- 登录 Session、游客登录、模拟微信登录、自动登录和退出登录。
- 大厅权威状态、在线人数、公告、服务器状态。
- 房间号、密码摘要、密码重试限制、创建/查询/加入。
- 自定义 PlayerState、房主、四座位、准备/取消准备、解散。
- 牌桌 Actor、回合管理、服务端发牌、摸牌、出牌推进。
- 多玩家操作窗口、胡/杠/碰优先级、抢杠胡、一炮多响。
- 结算到下一局完整流程。
- 会话恢复和断线快照。
- Dedicated Server + 4 客户端真实端到端验证。

## 编译与平台工具链

| 平台/目标 | 当前证据 | 状态 |
|---|---|---|
| Win64 Editor | `UnrealEditor-GuiyangMahjong.dll` 已生成 | 通过（上一阶段） |
| Win64 Game/Client | 当前无目标二进制 | 未验证 |
| Win64 Server | 当前无目标二进制 | 未验证 |
| Windows SDK | 10.0.22621.0 | VALID |
| Android | 平台验证报告 `INVALID r27c` | 阻塞 APK 构建 |
| Linux/LinuxArm64 | SDK 无效 | 非当前 MVP 优先项 |

## 当前自动化测试

已有 7 项：

- GuiyangMahjong.Core.Deck.Standard136
- GuiyangMahjong.Core.Deck.ShuffleAndDeal
- GuiyangMahjong.Rules.StandardHu
- GuiyangMahjong.Rules.QiDuiSwitch
- GuiyangMahjong.Rules.PengGang
- GuiyangMahjong.Rules.Ji
- GuiyangMahjong.Rules.ScoreZeroSum

这些测试只证明规则子集，不证明房间、RPC 安全、私有数据隔离、完整对局或四客户端可玩。

## 审计结论

当前工程是“可编译的规则与 UI 骨架”，尚不是总需求定义的 PC 可运行 MVP，也未达到真实可玩、四客户端验证或 Android 可部署状态。后续必须先完成登录、密码房和服务端房间模型，再把牌局流程接到 Dedicated Server，最后补齐 Widget、自动化和端到端证据。

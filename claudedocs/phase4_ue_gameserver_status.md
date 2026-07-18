# 阶段 4：UE Dedicated Server 接入状态

日期：2026-07-18

## 本阶段结论

Phase 4 的真实 UE 专用服务器接入和权威房间引导已经完成并通过端到端验收：Lobby 创建房间后，Allocator 能启动真实 `GuiyangMahjongServer`；Lobby 在注册应答中下发固定 RoomCode、房间属性和不可变规则快照，UE 校验并初始化唯一牌桌后才开始心跳并开放登录。Lobby 随后发布带签名 JoinTicket 的路由；实例崩溃后会被标记失败，端口可安全回收给后续房间。

标准 WindowsServer Cook/Stage/Pak 输出：

`Saved/StagedBuilds/Phase4Server/WindowsServer/GuiyangMahjong/Binaries/Win64/GuiyangMahjongServer.exe`

可复现构建入口：`Scripts/Build-Phase4Server.ps1`。

## 已实现链路

Allocator 启动 UE Server 时传入以下非敏感参数：

- `-MahjongManagedGameServer`
- `-RoomId=`、`-MatchId=`、`-ServerInstanceId=`
- `-Port=`、`-LobbyInternalUrl=`、`-BuildVersion=`、`-AdvertisedIp=`

共享签名密钥与一次性注册凭据不进入命令行，分别通过以下进程环境变量注入：

- `MAHJONG_JOIN_TICKET_SIGNING_KEY`
- `MAHJONG_REGISTRATION_CREDENTIAL`

UE Server 会：

1. 严格校验托管启动配置，失败时关闭入口。
2. 调用 Lobby `/internal/gameservers/register`，以一次性注册凭据换取心跳凭据。
3. 解析并校验注册应答中的权威房间定义；RoomId/MatchId 必须与进程启动范围一致，RoomCode 必须是固定六位数字，容量必须为四人。
4. 使用 Lobby 规则对象生成并验证 `FGuiyangRuleSnapshot`，在 `UGuiyangRoomManager` 内创建唯一托管房间；初始化成功前拒绝玩家并禁止心跳。
5. 注册并初始化成功后周期上报实例与房间状态。
6. 在 `PreLogin` 验证 JoinTicket 的 HMAC-SHA256 签名、玩家、房间、比赛、实例、过期时间和 nonce。
7. nonce 只能消费一次，并在 `InitNewPlayer` 将已经验证的 PlayerId 绑定到 PlayerController。
8. 后续认证 RPC 不允许把连接切换到其他 PlayerId；认证成功后只能进入本进程的权威房间。
9. 托管实例内的本地创建、快速开始和按房号加入入口全部拒绝，避免绕过 Lobby 权威。

客户端使用 `AGuiyangMahjongPlayerController::ConnectToAllocatedServer` 生成：

```text
server-ip:port?PlayerId=<url-encoded-player-id>?JoinTicket=<url-encoded-ticket>
```

客户端编码与服务端显式 URL 解码成对执行；日志不记录 JoinTicket、注册凭据、心跳凭据或签名密钥。

## 2026-07-18 验收证据

- UE Server Development 构建成功，最终二进制与恢复后的引擎源码一致。
- WindowsServer Cook/Stage/Pak：`BUILD SUCCESSFUL`。
- 暂存服务器加载 `/Engine/Maps/Entry` 并监听 UDP 游戏端口成功。
- UE 自动化：`GuiyangMahjong.GameServer.*` 共 4 项，4 成功、0 失败；新增权威 bootstrap 类型/范围失败路径和双实例房间隔离测试。
- 真实多进程联调：两个初始房间使用不同规则并分别产生不同 `RuleHash`；2 个 UE 实例持续心跳、占用 2 个唯一端口。终止实例后状态转为 Failed，第三个房间回收原端口，结果 `INTEGRATION_OK`。
- Allocator 测试 4/4、Lobby 测试 17/17；Fake GameServer 多进程回归仍通过并确认 2/2 实例注册后产生真实心跳。
- 最近相关日志扫描未发现签名密钥、注册凭据或敏感环境变量值。

真实联调入口：`Scripts/Test-Phase4ManagedServer.ps1`。脚本默认优先使用上述暂存服务器。

## 构建注意事项

当前源码引擎的旧 Editor receipt 会默认启用 Landmass/Water 等与无头服务器无关的实验性内容插件，其中包含不可用于 WindowsServer 的编辑器启动资源。因此服务器 Cook 使用 `-NoEnginePlugins` 隔离编辑器插件内容；Game、Editor、Server TargetRules 同时显式禁用 Landmass、Water、Volumetrics。

开发版单体 Server 仍静态包含若干默认插件类，其缺省 CDO 可能产生“插件默认资源未入包”的非致命告警；这不影响 Entry 地图、网络监听、托管注册、心跳或已完成的端到端验收。后续最小化 Shipping Server Target 时应继续裁剪默认插件集合。

## 下一批范围

Phase 4 剩余发布门禁：四个真实客户端凭 Lobby 路由和票据进入同一托管桌，人工完成整局、断线重连和结算。多桌继续采用“一桌一进程”，不允许一个 `GameState` 承载两桌。下一开发阶段进入 Phase 5 客户端/UI 远程大厅迁移。

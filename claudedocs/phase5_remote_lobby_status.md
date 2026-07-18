# Phase 5：UE 客户端与 UI 远程大厅迁移状态

日期：2026-07-18

## 本阶段结论

Phase 5 的第一批客户端迁移已经落地。`UGuiyangLobbySubsystem` 现在可以在
`LocalLegacy` 与 `RemoteLobby` 两种后端之间显式切换；远程模式通过 Lobby API v1
获取启动信息、公开房间目录、创建/加入结果以及带短期 JoinTicket 的 GameServer 路由。
收到合法路由后，客户端使用现有 `ConnectToAllocatedServer` 进行 `ClientTravel`。

生产默认仍为 `LocalLegacy`。独立 Auth 服务尚未签发可供 Lobby 验证的玩家 Bearer Token，
因此在认证链路和发布门禁完成前，不允许把 `RemoteLobby` 设为默认值，也不在 UE 客户端中
保存玩家签名密钥。

## 已实现链路

- 新增异步 `RemoteLobby` HTTP Backend，统一复用 `ILobbyBackend`，旧 RPC 链路未删除。
- 登录后从 Lobby 获取公告、显示名和在线人数，并刷新大厅 UI。
- 创建房间提交完整规则快照；密码只存在于 HTTPS 请求体，不写日志。
- 快速开始先选择可加入的公开、无密码、未满员等待房间，否则创建默认房间。
- 创建或加入返回 202 时轮询路由；路由就绪后验证玩家、房间、比赛、实例、有效期和端点。
- JoinTicket 和玩家身份由 Lobby 响应提供，客户端不能通过修改 IP、端口或房号绕过 GameServer `PreLogin`。
- 房间与结算界面的“返回大厅”在远程模式下断开 GameServer 并返回 Entry，大厅会话保留。
- HTTP 同步调度失败、网络失败、超时、401、服务错误和协议错误都会产生明确中文失败事件。

## 配置与安全边界

配置节：`/Script/GuiyangMahjong.GuiyangLobbySubsystem`

- `BackendMode=LocalLegacy|RemoteLobby`
- `RemoteBaseUrl=`
- `RemoteRequestTimeoutSeconds=10`
- `RemoteRoutePollIntervalSeconds=0.25`
- `RemoteRoutePollMaxAttempts=120`

命令行可使用 `-MahjongLobbyBackend=` 和 `-MahjongLobbyBaseUrl=` 覆盖。生产地址必须是
HTTPS；只有 `localhost`、`127.0.0.1` 和 `[::1]` 允许明文 HTTP，用于本机联调。
远程地址为空或不安全时严格失败，不静默回退到其他服务器。

## 2026-07-18 验证证据

- `GuiyangMahjongServer Win64 Development`：编译、链接成功。
- `GuiyangMahjongEditor`：本批 7 个项目源码单元编译成功；最终 DLL 写入被当前已打开的
  Unreal Editor 文件锁阻止，没有强制关闭用户的编辑器进程。
- UE 自动化 `GuiyangMahjong.Lobby.*`：3/3 成功，包括 `RemoteCodec`。
- Lobby .NET 测试：17/17 成功。
- Allocator .NET 测试：4/4 成功。
- Lobby + Allocator + 两个独立 Fake GameServer 回归：`INTEGRATION_OK`，两个实例使用独立端口，
  崩溃实例进入 Failed，端口随后可回收。
- `git diff --check`：通过；相关日志扫描未发现 JoinTicket、Bearer Token 或签名密钥输出。

## 尚未关闭的发布门禁

- 独立 Auth 应用签发生产玩家 Token，并完成过期、刷新和吊销策略。
- 打开编辑器的使用者自行关闭 Editor 后，补跑 Editor 最终 DLL 链接。
- PC 与 Android 真机从远程大厅进入真实 UE Dedicated Server 并返回大厅。
- 四个真实人工客户端完成同桌整局；断线重连与结算闭环属于 Phase 6。
- Android HTTPS 证书、正式域名、网络安全配置与发布签名验证。


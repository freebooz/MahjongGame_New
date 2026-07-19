# Phase 6：重连、结算与跨进程恢复闭环

日期：2026-07-18

## 阶段结论

Phase 6 中可由当前机器自动完成的控制面闭环已经完成。RemoteLobby 重连不复用旧 IP 或 JoinTicket，而是按已认证 `PlayerId` 查询权威牌桌并签发新票据。最终结算以 `MatchId + ResultSequence` 幂等持久化；GameServer 先将不含凭据的结果原子写入本地 outbox，再向 Lobby 补报。若 Lobby 不可用且 GameServer 同时崩溃，Allocator 会用内部服务身份恢复补报，成功后删除 outbox。

## 已实现链路

- Lobby 按 `PlayerId` 查询活动房间；客户端提供的 RoomId/MatchId 只作为过期提示，不能决定重连目标。
- UE `RetryLastConnection` 在 RemoteLobby 模式调用 `/v1/reconnect/route` 获取新的 JoinTicket。
- GameServer 注册返回房间作用域的 `resultCredential`；Lobby 仅保存 SHA-256 摘要。
- GameServer 最终结算包含 PlayerId、座位、排名和总分，并在 UI 展示前进入非阻塞补报链路。
- GameServer 先写 `version + matchId + report` 原子 outbox，再发 HTTP；文件不包含 Token、registrationCredential、heartbeatCredential 或 resultCredential。
- Allocator 为每个 `ServerInstanceId` 分配独立 outbox 路径，延迟扫描遗留文件，并以内部令牌调用 `/internal/matches/{matchId}/result/recovery`。
- Lobby 恢复接口同时校验 MatchId、RoomId、最后一次 ServerInstanceId、局数、玩家集合和结果序号；仅内部服务令牌可调用。
- 失败房间允许在权威恢复结果到达后执行 `Failed -> Closed`；恢复调用不反向 Drain Allocator，避免控制面循环。
- InMemory 与 Redis/PostgreSQL 适配器均以 `(match_id, result_sequence)` 去重：相同内容返回 Duplicate，不同内容返回 Conflict。
- 结算落库与关房原子完成；普通在线补报确认后由 Lobby 通知 Allocator 回收进程和端口。

## 验证证据

- `GuiyangMahjongServer Win64 Development`：UHT、项目源码编译、完整链接成功；可执行文件可正常启动。
- `GuiyangMahjong Win64 Development`：源码引擎首次完整 Game Target 构建与链接成功；零增量复验显示 `Target is up to date`，EXE SHA-256 为 `1292D7E7FD46969C912E8CEBFA47FC1223B6283777BE2FC1A5FCA693DDC05489`。
- UE GameServer 自动化：4/4 成功，日志为 `Saved/Logs/Phase6_GameServerOutbox.log`。
- UE Lobby 客户端契约自动化：3/3 成功，日志为 `Saved/Logs/Phase6_LobbyFinal.log`。
- Win64 完整 Cook/Stage/Pak 客户端启动成功，Stage 内再次执行 UE Lobby 契约自动化 3/3 成功、进程退出码 0，日志为 `Saved/Logs/Phase6_StagedWin64ClientFull_Lobby.log`。未 Cook 的 Game EXE 不作为可运行包验收。
- Lobby .NET 测试：22/22 成功，包含恢复接口鉴权、失败房间恢复和幂等结算。
- Allocator .NET 测试：6/6 成功，包含合法 outbox 补报删除、文件名/作用域不匹配拒绝和无凭据落盘检查。
- OpenAPI YAML：解析成功，11 条路径，包含恢复补报接口。
- 真实多进程故障门禁：`INTEGRATION_OK`。两台 UE Dedicated Server 独立注册并持续心跳；受害实例失败后暂停真实 Lobby 进程，确认 outbox 在中断期间留盘；恢复 Lobby 后 Allocator 补报成功、文件删除，端口 29100 被第三个房间安全复用。

## 安全与可用性边界

- 登录 Token、JoinTicket、registrationCredential、heartbeatCredential 与 resultCredential 均不得写日志或 outbox。
- outbox 最大 64 KiB，只扫描配置目录顶层的 `.json` 文件，并要求文件名等于报告中的 ServerInstanceId。
- 仅收到 Accepted 且 MatchId、ResultSequence 完全匹配的确认后删除 outbox；网络错误、非 2xx 或确认不匹配均保留文件重试。
- GameServer 内存重试与 Allocator 磁盘恢复可能并发，但 Lobby 的数据库唯一约束保证至多一次记分。

## 尚未关闭的发布门禁

- 在真实 Redis/PostgreSQL 环境执行 schema 升级、服务重启恢复和并发结算事务测试；当前机器未安装 Docker、Podman、psql、redis-server，不能伪造该结果。
- 四个真实客户端经 RemoteLobby 进行人工完整牌局，并在对局中切换网络验证新票据重连；该项需要四名人工操作或用户协调。
- 当前用户打开的 Unreal Editor（PID 5380）仍持有项目 DLL；关闭 Editor 后补跑 Editor 最终链接。独立 Win64 Game Target、完整 Stage 启动和客户端契约自动化已经通过，不再受该 DLL 锁影响。
- 源码引擎旧 `GuiyangMahjongEditor.target` receipt 仍把 Landmass 启动内容加入 Cook，严格 Cook 会因其引用 EditorOnly 材质产生 8 个错误。本轮完整客户端 Stage 仅为运行验证使用 `-IgnoreCookErrors` 放行这 8 个已知 Landmass 错误；Paper2D、HairStrands、MediaPlate 等客户端默认资源均已打入包且启动无 CDO 缺失。正式发布包必须在关闭 Editor、成功重建 Editor receipt 后以不含 `-IgnoreCookErrors` 的标准 Cook 再验收。
- ADB 工具可用，但当前没有已连接 Android 设备，因此本轮不能执行真实手机重装与移动端运行复验。
